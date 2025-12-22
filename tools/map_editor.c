#include "raylib.h"
#include "raymath.h" 
#include "map.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

// --- COMPILE INSTRUCTION ---
// gcc tools/map_editor.c src/map.c -o map_editor.exe -O2 -Wall -I src -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm

// --- CONFIGURATION ---
const float EDITOR_MAP_SCALE = 0.4f; 
// Check your folder casing! 'maps' vs 'Maps'
const char* MAP_FILE = "resources/maps/real_city.map"; 

// --- EDITOR STATE ---
typedef enum {
    STATE_VIEW,
    STATE_ADDING,
    STATE_NAMING,
    STATE_EDITING
} EditorState;

typedef struct {
    Camera2D camera;
    EditorState state;
    
    // Current Selection
    LocationType selectedType;
    Vector2 pendingPos;
    char pendingName[64];
    int nameLen;
    int editingIndex; 
    
    // UI State
    Rectangle typeButtons[11]; 
    const char* typeNames[11];
    Color typeColors[11];
} EditorData;

// --- HELPER: SAVE TO FILE ---
void SaveMapToFile(GameMap *map) {
    FILE *f = fopen(MAP_FILE, "w");
    if (f == NULL) {
        printf("ERROR: Could not open map file for saving!\n");
        return;
    }

    fprintf(f, "NODES:\n");
    for(int i=0; i<map->nodeCount; i++) {
        float rawX = map->nodes[i].position.x / EDITOR_MAP_SCALE;
        float rawY = map->nodes[i].position.y / EDITOR_MAP_SCALE;
        fprintf(f, "%d: %.2f %.2f %d\n", map->nodes[i].id, rawX, rawY, map->nodes[i].flags);
    }

    fprintf(f, "\nEDGES:\n");
    for(int i=0; i<map->edgeCount; i++) {
        float rawW = map->edges[i].width / EDITOR_MAP_SCALE;
        fprintf(f, "%d %d %.2f %d %d %d\n", 
            map->edges[i].startNode, map->edges[i].endNode, rawW, 
            map->edges[i].oneway, map->edges[i].maxSpeed, map->edges[i].lanes);
    }

    fprintf(f, "\nBUILDINGS:\n");
    for(int i=0; i<map->buildingCount; i++) {
        float rawH = map->buildings[i].height / EDITOR_MAP_SCALE;
        fprintf(f, "%.2f %d %d %d", rawH, map->buildings[i].color.r, map->buildings[i].color.g, map->buildings[i].color.b);
        for(int j=0; j<map->buildings[i].pointCount; j++) {
            float px = map->buildings[i].footprint[j].x / EDITOR_MAP_SCALE;
            float py = map->buildings[i].footprint[j].y / EDITOR_MAP_SCALE;
            fprintf(f, " %.2f %.2f", px, py);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\nAREAS:\n");
    for(int i=0; i<map->areaCount; i++) {
        fprintf(f, "%d %d %d %d", map->areas[i].type, map->areas[i].color.r, map->areas[i].color.g, map->areas[i].color.b);
        for(int j=0; j<map->areas[i].pointCount; j++) {
            float px = map->areas[i].points[j].x / EDITOR_MAP_SCALE;
            float py = map->areas[i].points[j].y / EDITOR_MAP_SCALE;
            fprintf(f, " %.2f %.2f", px, py);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\nL:\n");
    for(int i=0; i<map->locationCount; i++) {
        if(map->locations[i].type == LOC_NONE) continue; 

        float rawX = map->locations[i].position.x / EDITOR_MAP_SCALE;
        float rawY = map->locations[i].position.y / EDITOR_MAP_SCALE;
        
        char safeName[64];
        strncpy(safeName, map->locations[i].name, 64);
        for(int k=0; safeName[k]; k++) if(safeName[k] == ' ') safeName[k] = '_';
        
        fprintf(f, "L %d %.2f %.2f %s\n", map->locations[i].type, rawX, rawY, safeName);
    }

    fclose(f);
    printf("Map Saved Successfully!\n");
}

// --- MAIN EDITOR ---
int main(void) {
    InitWindow(1280, 720, "Map Editor - 'Unknown' Fix");
    SetTargetFPS(60);

    GameMap map = LoadGameMap(MAP_FILE); 
    
    EditorData editor = {0};
    editor.camera.zoom = 1.0f;
    editor.camera.offset = (Vector2){640, 360};
    editor.state = STATE_VIEW;
    editor.selectedType = LOC_HOUSE;
    editor.editingIndex = -1;
    
    // Setup UI
    const char* names[] = {"None", "Fuel", "FastFood", "Cafe", "Bar", "Market", "SuperMkt", "Rest.", "House", "Park", "Water"};
    
    // FIX: Colors adjusted for visibility
    // LOC_HOUSE (Index 8) changed from DARKGRAY to MAGENTA so you can see "Unknown" spots
    Color colors[] = {GRAY, ORANGE, RED, BROWN, PURPLE, BLUE, DARKBLUE, MAROON, MAGENTA, GREEN, SKYBLUE};
    
    for(int i=1; i<11; i++) { 
        editor.typeNames[i] = names[i];
        editor.typeColors[i] = colors[i];
        editor.typeButtons[i] = (Rectangle){ 10 + (i-1)*110, 650, 100, 40 };
    }

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mouse, editor.camera);

        // --- CONTROLS ---
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 delta = GetMouseDelta();
            Vector2 deltaWorld = Vector2Scale(delta, -1.0f/editor.camera.zoom);
            editor.camera.target = Vector2Add(editor.camera.target, deltaWorld);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouseWorldBefore = GetScreenToWorld2D(mouse, editor.camera);
            editor.camera.zoom += wheel * 0.25f;
            if (editor.camera.zoom < 0.1f) editor.camera.zoom = 0.1f;
            Vector2 mouseWorldAfter = GetScreenToWorld2D(mouse, editor.camera);
            editor.camera.target = Vector2Add(editor.camera.target, Vector2Subtract(mouseWorldBefore, mouseWorldAfter));
        }

        // --- INTERACTION LOGIC ---
        
        // 1. Check UI Clicks (Available in ALL states)
        if (mouse.y > 600) {
            for(int i=1; i<11; i++) {
                if (CheckCollisionPointRec(mouse, editor.typeButtons[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    editor.selectedType = (LocationType)i;
                    
                    // If we are currently editing a node, apply this type change immediately!
                    if (editor.state == STATE_EDITING && editor.editingIndex != -1) {
                        map.locations[editor.editingIndex].type = editor.selectedType;
                        map.locations[editor.editingIndex].iconID = (int)editor.selectedType;
                    } 
                    // Otherwise switch to Adding mode
                    else {
                        editor.state = STATE_ADDING;
                    }
                }
            }
        }
        // 2. Map Interaction
        else if (editor.state == STATE_VIEW || editor.state == STATE_ADDING) {
            // Find Clicked Location
            int clickedIndex = -1;
            for(int i=0; i<map.locationCount; i++) {
                if (map.locations[i].type == LOC_NONE) continue;
                if (CheckCollisionPointCircle(worldMouse, map.locations[i].position, 5.0f / editor.camera.zoom)) {
                    clickedIndex = i;
                    break;
                }
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (clickedIndex != -1) {
                    // Start Editing Existing
                    editor.state = STATE_EDITING;
                    editor.editingIndex = clickedIndex;
                    editor.pendingPos = map.locations[clickedIndex].position;
                    strncpy(editor.pendingName, map.locations[clickedIndex].name, 64);
                    editor.nameLen = strlen(editor.pendingName);
                    editor.selectedType = map.locations[clickedIndex].type;
                } else {
                    // Add New
                    editor.state = STATE_NAMING;
                    editor.editingIndex = -1;
                    editor.pendingPos = worldMouse;
                    editor.nameLen = 0;
                    memset(editor.pendingName, 0, 64);
                    
                    if (editor.selectedType == LOC_HOUSE) {
                        // Fast add for houses
                        snprintf(editor.pendingName, 64, "House_%d", map.locationCount);
                        map.locations[map.locationCount].position = editor.pendingPos;
                        map.locations[map.locationCount].type = LOC_HOUSE;
                        strncpy(map.locations[map.locationCount].name, editor.pendingName, 64);
                        map.locationCount++;
                        SaveMapToFile(&map);
                        editor.state = STATE_ADDING;
                    }
                }
            }
            else if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
                if (clickedIndex != -1) {
                    map.locations[clickedIndex].type = LOC_NONE; 
                    SaveMapToFile(&map);
                }
            }
        }
        else if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
            // Typing Logic
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (editor.nameLen < 32)) {
                    editor.pendingName[editor.nameLen] = (char)key;
                    editor.pendingName[editor.nameLen+1] = '\0';
                    editor.nameLen++;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && editor.nameLen > 0) {
                editor.nameLen--;
                editor.pendingName[editor.nameLen] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) && editor.nameLen > 0) {
                if (editor.state == STATE_NAMING) {
                    if (map.locationCount < MAX_LOCATIONS) {
                        MapLocation *l = &map.locations[map.locationCount];
                        strncpy(l->name, editor.pendingName, 64);
                        l->position = editor.pendingPos;
                        l->type = editor.selectedType;
                        map.locationCount++;
                    }
                } else {
                    MapLocation *l = &map.locations[editor.editingIndex];
                    strncpy(l->name, editor.pendingName, 64);
                    l->type = editor.selectedType;
                }
                SaveMapToFile(&map);
                editor.state = STATE_ADDING; 
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                editor.state = STATE_ADDING; 
            }
        }

        // --- DRAWING ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode2D(editor.camera);
                float scale = 1.0f / editor.camera.zoom;

                // Draw Areas
                for (int i=0; i<map.areaCount; i++) {
                    if(map.areas[i].pointCount < 3) continue;
                    Vector2 center = {0,0};
                    for(int j=0; j<map.areas[i].pointCount; j++) center = Vector2Add(center, map.areas[i].points[j]);
                    center = Vector2Scale(center, 1.0f/map.areas[i].pointCount);
                    for (int j = 0; j < map.areas[i].pointCount; j++) {
                        Vector2 p1 = map.areas[i].points[j];
                        Vector2 p2 = map.areas[i].points[(j+1)%map.areas[i].pointCount];
                        DrawTriangle(center, p2, p1, Fade(map.areas[i].color, 0.3f));
                    }
                }

                // Draw Roads
                for (int i = 0; i < map.edgeCount; i++) {
                    Vector2 s = map.nodes[map.edges[i].startNode].position;
                    Vector2 e = map.nodes[map.edges[i].endNode].position;
                    DrawLineEx(s, e, map.edges[i].width, LIGHTGRAY);
                }
                
                // Draw Buildings
                for (int i = 0; i < map.buildingCount; i++) {
                    DrawTriangleFan(map.buildings[i].footprint, map.buildings[i].pointCount, map.buildings[i].color);
                }
                
                // Draw POIs
                for(int i=0; i<map.locationCount; i++) {
                    if(map.locations[i].type == LOC_NONE) continue;
                    
                    Color c = editor.typeColors[map.locations[i].type];
                    
                    // Draw Circle
                    DrawCircleV(map.locations[i].position, 3.0f * scale, c);
                    DrawCircleLines(map.locations[i].position.x, map.locations[i].position.y, 3.2f * scale, BLACK);
                    
                    // Always draw name if editing or hovering
                    bool hover = CheckCollisionPointCircle(worldMouse, map.locations[i].position, 5.0f * scale);
                    if (editor.camera.zoom > 0.5f || hover || editor.editingIndex == i) {
                        const char* txt = map.locations[i].name;
                        // Special indicator for Unknown
                        if (strcmp(txt, "Unknown") == 0) txt = "Unknown (?)";
                        
                        DrawText(txt, 
                                 map.locations[i].position.x, 
                                 map.locations[i].position.y - (10 * scale), 
                                 10 * scale, BLACK);
                    }
                }
                
                if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                    DrawCircleV(editor.pendingPos, 4.0f * scale, RED);
                    DrawCircleLines(editor.pendingPos.x, editor.pendingPos.y, 4.5f * scale, BLACK);
                }

            EndMode2D();

            // UI DRAWING
            DrawRectangle(0, 600, 1280, 120, LIGHTGRAY);
            DrawLine(0, 600, 1280, 600, GRAY);
            
            for(int i=1; i<11; i++) {
                Rectangle btn = editor.typeButtons[i];
                bool isSelected = (editor.selectedType == i);
                DrawRectangleRec(btn, isSelected ? WHITE : editor.typeColors[i]);
                DrawRectangleLinesEx(btn, isSelected ? 3 : 1, BLACK);
                
                int txtW = MeasureText(editor.typeNames[i], 10);
                DrawText(editor.typeNames[i], btn.x + (btn.width - txtW)/2, btn.y + 15, 10, isSelected ? BLACK : WHITE);
            }
            
            DrawText("Left: Edit/Add | Middle: Delete | Right: Pan | Scroll: Zoom", 10, 610, 10, DARKGRAY);

            if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                DrawRectangle(400, 200, 480, 120, Fade(WHITE, 0.95f));
                DrawRectangleLines(400, 200, 480, 120, BLACK);
                const char* title = (editor.state == STATE_EDITING) ? "EDITING LOCATION:" : "NEW LOCATION:";
                DrawText(title, 420, 220, 20, GRAY);
                
                // Show Type
                DrawText(TextFormat("Type: %s", editor.typeNames[editor.selectedType]), 420, 245, 10, editor.typeColors[editor.selectedType]);
                
                DrawText(editor.pendingName, 420, 260, 30, BLACK);
                if ((GetTime()*2) - (int)(GetTime()*2) < 0.5f) {
                    DrawRectangle(420 + MeasureText(editor.pendingName, 30) + 2, 260, 2, 30, BLACK);
                }
                DrawText("Press ENTER to Save, ESC to Cancel", 420, 300, 10, DARKGRAY);
            }

        EndDrawing();
    }

    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
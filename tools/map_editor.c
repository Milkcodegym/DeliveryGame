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
const char* MAP_FILE = "resources/maps/whole_city.map"; 

#define LOC_DELETED -1 

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
    Rectangle typeButtons[LOC_COUNT]; 
    const char* typeNames[LOC_COUNT];
    Color typeColors[LOC_COUNT];
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
        // Save with 1 decimal precision to save space
        fprintf(f, "%d: %.1f %.1f %d\n", map->nodes[i].id, rawX, rawY, map->nodes[i].flags);
    }

    fprintf(f, "\nEDGES:\n");
    for(int i=0; i<map->edgeCount; i++) {
        float rawW = map->edges[i].width / EDITOR_MAP_SCALE;
        fprintf(f, "%d %d %.1f %d %d %d\n", 
            map->edges[i].startNode, map->edges[i].endNode, rawW, 
            map->edges[i].oneway, map->edges[i].maxSpeed, 0); 
    }

    fprintf(f, "\nBUILDINGS:\n");
    for(int i=0; i<map->buildingCount; i++) {
        float rawH = map->buildings[i].height / EDITOR_MAP_SCALE;
        fprintf(f, "%.1f %d %d %d", rawH, map->buildings[i].color.r, map->buildings[i].color.g, map->buildings[i].color.b);
        for(int j=0; j<map->buildings[i].pointCount; j++) {
            float px = map->buildings[i].footprint[j].x / EDITOR_MAP_SCALE;
            float py = map->buildings[i].footprint[j].y / EDITOR_MAP_SCALE;
            fprintf(f, " %.1f %.1f", px, py);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\nAREAS:\n");
    for(int i=0; i<map->areaCount; i++) {
        fprintf(f, "%d %d %d %d", map->areas[i].type, map->areas[i].color.r, map->areas[i].color.g, map->areas[i].color.b);
        for(int j=0; j<map->areas[i].pointCount; j++) {
            float px = map->areas[i].points[j].x / EDITOR_MAP_SCALE;
            float py = map->areas[i].points[j].y / EDITOR_MAP_SCALE;
            fprintf(f, " %.1f %.1f", px, py);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "\nL:\n");
    for(int i=0; i<map->locationCount; i++) {
        if((int)map->locations[i].type == LOC_DELETED) continue; 

        float rawX = map->locations[i].position.x / EDITOR_MAP_SCALE;
        float rawY = map->locations[i].position.y / EDITOR_MAP_SCALE;
        
        char safeName[64];
        strncpy(safeName, map->locations[i].name, 64);
        for(int k=0; safeName[k]; k++) if(safeName[k] == ' ') safeName[k] = '_';
        
        fprintf(f, "L %d %.1f %.1f %s\n", map->locations[i].type, rawX, rawY, safeName);
    }

    fclose(f);
    printf("Map Saved Successfully!\n");
}

// --- MAIN EDITOR ---
int main(void) {
    InitWindow(1280, 720, "Map Editor - v2.0 (Optimized)");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    GameMap map = LoadGameMap(MAP_FILE); 
    
    EditorData editor = {0};
    editor.camera.zoom = 1.0f;
    editor.camera.offset = (Vector2){GetScreenWidth()/2.0f, GetScreenHeight()/2.0f};
    editor.state = STATE_VIEW;
    editor.selectedType = LOC_FUEL; 
    editor.editingIndex = -1;
    
    // [FIX] Auto-Jump to the first node so we don't look at empty void
    if (map.nodeCount > 0) {
        editor.camera.target = map.nodes[0].position;
        printf("EDITOR: Camera jumped to Node 0 (%.1f, %.1f)\n", map.nodes[0].position.x, map.nodes[0].position.y);
    } else {
        editor.camera.target = (Vector2){0,0};
        printf("EDITOR: Warning - Map is empty. At 0,0.\n");
    }
    
    const char* names[LOC_COUNT] = {
        "Fuel", "FastFood", "Cafe", "Bar", "Market", "SuperMkt", "Rest.", "House", "Mechanic"
    };
    
    Color colors[LOC_COUNT] = {
        ORANGE, RED, BROWN, PURPLE, BLUE, DARKBLUE, MAROON, MAGENTA, BLACK
    };
    
    for(int i=0; i<LOC_COUNT; i++) { 
        editor.typeNames[i] = names[i];
        editor.typeColors[i] = colors[i];
    }

    while (!WindowShouldClose()) {
        int scrW = GetScreenWidth();
        int scrH = GetScreenHeight();
        
        // --- SCALING LOGIC ---
        float uiScale = (float)scrH / 720.0f;
        int uiHeight = (int)(140 * uiScale);
        int uiTop = scrH - uiHeight;
        
        // Keep camera center focused when resizing window
        editor.camera.offset = (Vector2){scrW/2.0f, scrH/2.0f};

        int btnWidth = (int)(100 * uiScale);
        int btnHeight = (int)(40 * uiScale);
        int spacing = (int)(10 * uiScale);
        int startX = (int)(10 * uiScale);
        
        for(int i=0; i<LOC_COUNT; i++) { 
            editor.typeButtons[i] = (Rectangle){ 
                (float)(startX + i*(btnWidth+spacing)), 
                (float)(uiTop + (50 * uiScale)), 
                (float)btnWidth, 
                (float)btnHeight 
            };
        }

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

        // --- INTERACTION ---
        if (mouse.y > uiTop) {
            for(int i=0; i<LOC_COUNT; i++) {
                if (CheckCollisionPointRec(mouse, editor.typeButtons[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    editor.selectedType = (LocationType)i;
                    if (editor.state == STATE_EDITING && editor.editingIndex != -1) {
                        map.locations[editor.editingIndex].type = editor.selectedType;
                        map.locations[editor.editingIndex].iconID = (int)editor.selectedType;
                    } else {
                        editor.state = STATE_ADDING;
                    }
                }
            }
        }
        else if (editor.state == STATE_VIEW || editor.state == STATE_ADDING) {
            int clickedIndex = -1;
            float hitRadius = (10.0f / editor.camera.zoom) * uiScale;
            if (hitRadius < 2.0f) hitRadius = 2.0f;

            for(int i=0; i<map.locationCount; i++) {
                if ((int)map.locations[i].type == LOC_DELETED) continue;
                if (CheckCollisionPointCircle(worldMouse, map.locations[i].position, hitRadius)) {
                    clickedIndex = i;
                    break;
                }
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (clickedIndex != -1) {
                    editor.state = STATE_EDITING;
                    editor.editingIndex = clickedIndex;
                    editor.pendingPos = map.locations[clickedIndex].position;
                    strncpy(editor.pendingName, map.locations[clickedIndex].name, 64);
                    editor.nameLen = strlen(editor.pendingName);
                    editor.selectedType = map.locations[clickedIndex].type;
                } else {
                    editor.state = STATE_NAMING;
                    editor.editingIndex = -1;
                    editor.pendingPos = worldMouse;
                    editor.nameLen = 0;
                    memset(editor.pendingName, 0, 64);
                    
                    if (editor.selectedType == LOC_HOUSE) {
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
                    map.locations[clickedIndex].type = (LocationType)LOC_DELETED; 
                    SaveMapToFile(&map);
                }
            }
        }
        else if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
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
            if (IsKeyPressed(KEY_F1)) {
                editor.state = STATE_ADDING; 
            }
        }

        // --- DRAWING ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode2D(editor.camera);
                float drawScale = 1.0f / editor.camera.zoom;

                // Draw Origin Grid (To help visualize scale and center)
                DrawLine(-1000, 0, 1000, 0, Fade(RED, 0.5f)); // X Axis
                DrawLine(0, -1000, 0, 1000, Fade(GREEN, 0.5f)); // Y Axis

                // Areas
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

                // Roads
                for (int i = 0; i < map.edgeCount; i++) {
                    Vector2 s = map.nodes[map.edges[i].startNode].position;
                    Vector2 e = map.nodes[map.edges[i].endNode].position;
                    DrawLineEx(s, e, map.edges[i].width, LIGHTGRAY);
                }
                
                // Buildings
                // Buildings
                for (int i = 0; i < map.buildingCount; i++) {
                    // [FIX] Use Outlines instead of TriangleFan
                    // TriangleFan creates glitches on L-shaped buildings. 
                    // Lines show the true shape of the data.
                    
                    if (map.buildings[i].pointCount < 2) continue;

                    Color bColor = map.buildings[i].color;
                    
                    // Draw filled shape (Optional: uses Raylib's simple triangulation)
                    // DrawTriangleFan(map.buildings[i].footprint, map.buildings[i].pointCount, Fade(bColor, 0.5f));

                    // Draw Thick Outline (The Truth)
                    DrawLineStrip(map.buildings[i].footprint, map.buildings[i].pointCount, bColor);
                    
                    // Close the loop (Connect last point to first)
                    Vector2 start = map.buildings[i].footprint[0];
                    Vector2 end = map.buildings[i].footprint[map.buildings[i].pointCount - 1];
                    DrawLineEx(start, end, 2.0f, bColor); // 2.0f thickness
                }
                
                // POIs (Scaled Pins)
                float pinRadius = (6.0f * drawScale) * uiScale; 
                if (pinRadius < 2.0f) pinRadius = 2.0f;
                if (pinRadius > 20.0f) pinRadius = 20.0f;

                for(int i=0; i<map.locationCount; i++) {
                    if((int)map.locations[i].type == LOC_DELETED) continue;
                    Color c = editor.typeColors[map.locations[i].type];
                    
                    DrawCircleV(map.locations[i].position, pinRadius, c);
                    DrawCircleLines(map.locations[i].position.x, map.locations[i].position.y, pinRadius, BLACK);
                    
                    bool hover = CheckCollisionPointCircle(worldMouse, map.locations[i].position, pinRadius * 1.5f);
                    if (editor.camera.zoom > 0.5f || hover || editor.editingIndex == i) {
                        float textSize = (20.0f * drawScale) * uiScale;
                        if (textSize < 10) textSize = 10; 
                        if (textSize > 50) textSize = 50;

                        DrawText(map.locations[i].name, 
                                 map.locations[i].position.x, 
                                 map.locations[i].position.y - (pinRadius * 2), 
                                 (int)textSize, BLACK);
                    }
                }
                
                if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                    DrawCircleV(editor.pendingPos, pinRadius * 1.2f, RED);
                    DrawCircleLines(editor.pendingPos.x, editor.pendingPos.y, pinRadius * 1.2f, BLACK);
                }

            EndMode2D();

            // UI DRAWING
            DrawRectangle(0, uiTop, scrW, uiHeight, LIGHTGRAY);
            DrawLine(0, uiTop, scrW, uiTop, GRAY);
            
            for(int i=0; i<LOC_COUNT; i++) {
                Rectangle btn = editor.typeButtons[i];
                bool isSelected = (editor.selectedType == i);
                DrawRectangleRec(btn, isSelected ? WHITE : editor.typeColors[i]);
                DrawRectangleLinesEx(btn, isSelected ? 3 : 1, BLACK);
                
                int fontSize = (int)(18 * uiScale);
                if (fontSize < 10) fontSize = 10;
                
                int txtW = MeasureText(editor.typeNames[i], fontSize);
                DrawText(editor.typeNames[i], btn.x + (btn.width - txtW)/2, btn.y + (btn.height - fontSize)/2, fontSize, isSelected ? BLACK : WHITE);
            }
            
            int helpFontSize = (int)(16 * uiScale);
            DrawText("Left: Edit/Add | Middle: Delete | Right: Pan | Scroll: Zoom", 10, uiTop + 10, helpFontSize, DARKGRAY);
            // Debug Text for Camera
            DrawText(TextFormat("Cam: %.1f, %.1f | Zoom: %.2f", editor.camera.target.x, editor.camera.target.y, editor.camera.zoom), 10, 10, 20, DARKGRAY);

            if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                float mw = 480 * uiScale; float mh = 120 * uiScale;
                float mx = (scrW - mw)/2; float my = (uiTop - mh)/2; 
                
                DrawRectangle(mx, my, mw, mh, Fade(WHITE, 0.95f));
                DrawRectangleLines(mx, my, mw, mh, BLACK);
                
                int titleSize = (int)(20 * uiScale);
                int inputSize = (int)(30 * uiScale);
                
                const char* title = (editor.state == STATE_EDITING) ? "EDITING LOCATION:" : "NEW LOCATION:";
                DrawText(title, mx + 20*uiScale, my + 20*uiScale, titleSize, GRAY);
                
                DrawText(TextFormat("Type: %s", editor.typeNames[editor.selectedType]), mx + 20*uiScale, my + 45*uiScale, (int)(14*uiScale), editor.typeColors[editor.selectedType]);
                
                DrawText(editor.pendingName, mx + 20*uiScale, my + 60*uiScale, inputSize, BLACK);
                if ((GetTime()*2) - (int)(GetTime()*2) < 0.5f) {
                    DrawRectangle(mx + 20*uiScale + MeasureText(editor.pendingName, inputSize) + 2, my + 60*uiScale, 2, inputSize, BLACK);
                }
                DrawText("Press ENTER to Save, F1 to Cancel", mx + 20*uiScale, my + 100*uiScale, (int)(12*uiScale), DARKGRAY);
            }

        EndDrawing();
    }

    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
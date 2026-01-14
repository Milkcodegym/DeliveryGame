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
const char* MAP_FILE = "resources/maps/smaller_city.map"; 

#define LOC_DELETED -1 
#define EDITOR_LOC_COUNT 10 

// --- BOUNDARY DATA ---
typedef struct {
    Vector2 start;
    Vector2 end;
} EditorBoundary;

#define MAX_EDITOR_BOUNDARIES 1024
static EditorBoundary boundaries[MAX_EDITOR_BOUNDARIES];
static int boundaryCount = 0;

// --- EDITOR STATE ---
typedef enum {
    STATE_VIEW,
    STATE_ADDING,
    STATE_NAMING,
    STATE_EDITING,
    STATE_DRAWING_BORDER,
    STATE_DRAWING_ZONE // [NEW] Zone Drawing Mode
} EditorState;

typedef struct {
    Camera2D camera;
    EditorState state;
    
    // Current Selection
    int selectedType; 
    Vector2 pendingPos;
    char pendingName[64];
    int nameLen;
    int editingIndex; 
    
    // Border Tool State
    Vector2 lastBorderPoint;
    bool hasStartPoint;

    // [NEW] Zone Tool State
    Vector2 zonePoints[64];
    int zonePointCount;

    // UI State
    Rectangle typeButtons[EDITOR_LOC_COUNT]; 
    const char* typeNames[EDITOR_LOC_COUNT];
    Color typeColors[EDITOR_LOC_COUNT];
} EditorData;

// --- FILE I/O ---

void LoadBoundaries() {
    FILE *f = fopen(MAP_FILE, "r");
    if (!f) return;
    
    char line[256];
    bool reading = false;
    boundaryCount = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "BOUNDARIES:", 11) == 0) {
            reading = true;
            continue;
        }
        if (reading) {
            if (line[0] == '\n' || line[0] == ' ' || strlen(line) < 5) {
                if (line[0] >= 'A' && line[0] <= 'Z') break; 
            }
            
            float x1, y1, x2, y2;
            if (sscanf(line, "%f %f %f %f", &x1, &y1, &x2, &y2) == 4) {
                if (boundaryCount < MAX_EDITOR_BOUNDARIES) {
                    boundaries[boundaryCount].start = (Vector2){x1 * EDITOR_MAP_SCALE, y1 * EDITOR_MAP_SCALE};
                    boundaries[boundaryCount].end   = (Vector2){x2 * EDITOR_MAP_SCALE, y2 * EDITOR_MAP_SCALE};
                    boundaryCount++;
                }
            }
        }
    }
    fclose(f);
    printf("Loaded %d map boundaries.\n", boundaryCount);
}

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
        // Type 1 = Park, Type 2 = Water (We can use type for differentiation later)
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

    fprintf(f, "\nBOUNDARIES:\n");
    for(int i=0; i<boundaryCount; i++) {
        float x1 = boundaries[i].start.x / EDITOR_MAP_SCALE;
        float y1 = boundaries[i].start.y / EDITOR_MAP_SCALE;
        float x2 = boundaries[i].end.x / EDITOR_MAP_SCALE;
        float y2 = boundaries[i].end.y / EDITOR_MAP_SCALE;
        fprintf(f, "%.1f %.1f %.1f %.1f\n", x1, y1, x2, y2);
    }

    fclose(f);
    printf("Map Saved Successfully!\n");
}

bool IsVisible(Camera2D cam, Vector2 pos, float margin) {
    Vector2 min = GetScreenToWorld2D((Vector2){0 - margin, 0 - margin}, cam);
    Vector2 max = GetScreenToWorld2D((Vector2){GetScreenWidth() + margin, GetScreenHeight() + margin}, cam);
    return (pos.x >= min.x && pos.x <= max.x && pos.y >= min.y && pos.y <= max.y);
}

// --- MAIN EDITOR ---
int main(void) {
    InitWindow(1280, 720, "Map Editor v3.0 (Zones + Save)");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    GameMap map = LoadGameMap(MAP_FILE); 
    LoadBoundaries();

    EditorData editor = {0};
    editor.camera.zoom = 1.0f;
    editor.camera.offset = (Vector2){GetScreenWidth()/2.0f, GetScreenHeight()/2.0f};
    editor.state = STATE_VIEW;
    editor.selectedType = LOC_FUEL; 
    editor.editingIndex = -1;
    
    if (map.nodeCount > 0) editor.camera.target = map.nodes[0].position;
    
    const char* names[EDITOR_LOC_COUNT] = {
        "Fuel", "Food", "Cafe", "Bar", "Market", "SuperMkt", "Rest.", "House", "Mechanic", "DEALER"
    };
    Color colors[EDITOR_LOC_COUNT] = {
        ORANGE, RED, BROWN, PURPLE, BLUE, DARKBLUE, MAROON, MAGENTA, BLACK, GOLD
    };
    
    for(int i=0; i<EDITOR_LOC_COUNT; i++) { 
        editor.typeNames[i] = names[i];
        editor.typeColors[i] = colors[i];
    }

    // Save notification
    float saveTimer = 0.0f;

    while (!WindowShouldClose()) {
        int scrW = GetScreenWidth();
        int scrH = GetScreenHeight();
        float dt = GetFrameTime();
        
        if (saveTimer > 0) saveTimer -= dt;

        float scaleX = (float)scrW / 1280.0f;
        float scaleY = (float)scrH / 720.0f;
        float uiScale = (scaleX < scaleY) ? scaleX : scaleY; 
        if (uiScale < 0.5f) uiScale = 0.5f;

        int uiHeight = (int)(150 * uiScale);
        int uiTop = scrH - uiHeight;
        
        editor.camera.offset = (Vector2){scrW/2.0f, scrH/2.0f};

        int btnWidth = (int)(105 * uiScale);
        int btnHeight = (int)(40 * uiScale);
        int spacing = (int)(8 * uiScale);
        int startX = (int)(15 * uiScale);
        
        int totalW = EDITOR_LOC_COUNT * (btnWidth + spacing) + startX;
        if (totalW > scrW) {
            float squash = (float)scrW / (float)totalW;
            btnWidth = (int)(btnWidth * squash * 0.95f);
            spacing = (int)(spacing * squash);
        }

        for(int i=0; i<EDITOR_LOC_COUNT; i++) { 
            editor.typeButtons[i] = (Rectangle){ 
                (float)(startX + i*(btnWidth+spacing)), 
                (float)(uiTop + (55 * uiScale)), 
                (float)btnWidth, 
                (float)btnHeight 
            };
        }

        Vector2 mouse = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mouse, editor.camera);

        // --- CAMERA CONTROLS ---
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

        // --- HOTKEYS ---
        if (IsKeyPressed(KEY_F1)) { editor.state = STATE_VIEW; editor.hasStartPoint = false; editor.zonePointCount = 0; }
        if (IsKeyPressed(KEY_F2)) { editor.state = STATE_DRAWING_BORDER; editor.hasStartPoint = false; }
        if (IsKeyPressed(KEY_F3)) { editor.state = STATE_DRAWING_ZONE; editor.zonePointCount = 0; } // [NEW] F3 for Zones

        // [NEW] CTRL+S SAVE
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
            SaveMapToFile(&map);
            saveTimer = 2.0f; // Show "Saved!" for 2 seconds
        }

        // --- MOUSE INTERACTION ---
        if (mouse.y > uiTop) {
            for(int i=0; i<EDITOR_LOC_COUNT; i++) {
                if (CheckCollisionPointRec(mouse, editor.typeButtons[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    editor.selectedType = i;
                    if (editor.state == STATE_EDITING && editor.editingIndex != -1) {
                        map.locations[editor.editingIndex].type = (LocationType)i;
                    } else {
                        editor.state = STATE_ADDING;
                        editor.hasStartPoint = false; 
                    }
                }
            }
        }
        else {
            // --- MODE: DRAW ZONE (F3) ---
            if (editor.state == STATE_DRAWING_ZONE) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (editor.zonePointCount < 64) {
                        editor.zonePoints[editor.zonePointCount++] = worldMouse;
                    }
                }
                
                // Right Click or Enter to Finish and Save
                if ((IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || IsKeyPressed(KEY_ENTER)) && editor.zonePointCount >= 3) {
                    if (map.areaCount < MAX_AREAS) {
                        MapArea *newArea = &map.areas[map.areaCount];
                        newArea->type = 2; // Type 2 = WATER
                        newArea->color = (Color){0, 121, 241, 255}; // Deep Blue
                        newArea->pointCount = editor.zonePointCount;
                        newArea->points = (Vector2*)malloc(sizeof(Vector2) * editor.zonePointCount);
                        
                        for(int i=0; i<editor.zonePointCount; i++) {
                            newArea->points[i] = editor.zonePoints[i];
                        }
                        
                        map.areaCount++;
                        printf("Added new Water Zone with %d points.\n", editor.zonePointCount);
                    }
                    editor.zonePointCount = 0; // Reset
                }
            }
            // --- MODE: DRAW BORDER (F2) ---
            else if (editor.state == STATE_DRAWING_BORDER) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (!editor.hasStartPoint) {
                        editor.lastBorderPoint = worldMouse;
                        editor.hasStartPoint = true;
                    } else {
                        if (boundaryCount < MAX_EDITOR_BOUNDARIES) {
                            boundaries[boundaryCount].start = editor.lastBorderPoint;
                            boundaries[boundaryCount].end = worldMouse;
                            boundaryCount++;
                            editor.lastBorderPoint = worldMouse; 
                        }
                    }
                }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) editor.hasStartPoint = false;
                
                if (IsKeyPressed(KEY_Z) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
                    if (boundaryCount > 0) boundaryCount--;
                    editor.hasStartPoint = false;
                }
            }
            // --- MODE: EDIT/ADD LOCATIONS ---
            else if (editor.state == STATE_VIEW || editor.state == STATE_ADDING) {
                int clickedIndex = -1;
                float hitRadius = (15.0f / editor.camera.zoom);
                if (hitRadius < 5.0f) hitRadius = 5.0f;

                for(int i=0; i<map.locationCount; i++) {
                    if ((int)map.locations[i].type == LOC_DELETED) continue;
                    if (CheckCollisionPointCircle(worldMouse, map.locations[i].position, hitRadius)) {
                        clickedIndex = i; break;
                    }
                }

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (clickedIndex != -1) {
                        editor.state = STATE_EDITING;
                        editor.editingIndex = clickedIndex;
                        editor.pendingPos = map.locations[clickedIndex].position;
                        strncpy(editor.pendingName, map.locations[clickedIndex].name, 64);
                        editor.nameLen = strlen(editor.pendingName);
                        editor.selectedType = (int)map.locations[clickedIndex].type;
                    } else {
                        editor.state = STATE_NAMING;
                        editor.editingIndex = -1;
                        editor.pendingPos = worldMouse;
                        editor.nameLen = 0;
                        memset(editor.pendingName, 0, 64);
                        if (editor.selectedType == LOC_HOUSE) {
                            snprintf(editor.pendingName, 64, "House_%d", map.locationCount);
                        }
                    }
                }
                if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) && clickedIndex != -1) {
                    map.locations[clickedIndex].type = (LocationType)LOC_DELETED; 
                }
            }
        }

        // --- KEYBOARD INPUT (NAMING) ---
        if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125) && (editor.nameLen < 32)) {
                    editor.pendingName[editor.nameLen++] = (char)key;
                    editor.pendingName[editor.nameLen] = '\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && editor.nameLen > 0) {
                editor.pendingName[--editor.nameLen] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER)) {
                MapLocation *l;
                if (editor.state == STATE_NAMING) {
                    if (map.locationCount < MAX_LOCATIONS) {
                        l = &map.locations[map.locationCount++];
                        l->position = editor.pendingPos;
                    } else l = NULL;
                } else {
                    l = &map.locations[editor.editingIndex];
                }
                
                if (l) {
                    strncpy(l->name, editor.pendingName, 64);
                    l->type = (LocationType)editor.selectedType;
                    if (l->type == LOC_DEALERSHIP) l->iconID = 9; 
                    else l->iconID = (int)l->type;
                    
                    SaveMapToFile(&map);
                }
                editor.state = STATE_ADDING;
            }
        }

        // --- RENDER ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode2D(editor.camera);
                
                float worldW = GetScreenWidth() / editor.camera.zoom;
                if (worldW < 3000) {
                    DrawLine(-3000, 0, 3000, 0, Fade(RED, 0.5f));
                    DrawLine(0, -3000, 0, 3000, Fade(GREEN, 0.5f));
                }

                // 1. Areas
                for (int i=0; i<map.areaCount; i++) {
                    if (map.areas[i].pointCount > 0 && IsVisible(editor.camera, map.areas[i].points[0], 500)) {
                        // Fill Area
                        Color c = map.areas[i].color;
                        // Use TriFan for visual fill (approximation)
                        DrawTriangleFan(map.areas[i].points, map.areas[i].pointCount, Fade(c, 0.3f));
                        DrawLineStrip(map.areas[i].points, map.areas[i].pointCount, Fade(c, 0.8f));
                        // Close loop visually
                        DrawLineEx(map.areas[i].points[map.areas[i].pointCount-1], map.areas[i].points[0], 2.0f, Fade(c, 0.8f));
                    }
                }

                // [NEW] Draw Active Zone Being Created
                if (editor.state == STATE_DRAWING_ZONE && editor.zonePointCount > 0) {
                    for(int i=0; i<editor.zonePointCount-1; i++) {
                        DrawLineEx(editor.zonePoints[i], editor.zonePoints[i+1], 2.0f, BLUE);
                        DrawCircleV(editor.zonePoints[i], 3.0f, BLUE);
                    }
                    // Draw line to cursor
                    DrawLineEx(editor.zonePoints[editor.zonePointCount-1], worldMouse, 2.0f, Fade(BLUE, 0.5f));
                    DrawCircleV(editor.zonePoints[editor.zonePointCount-1], 3.0f, BLUE);
                }

                // 2. Roads
                for (int i = 0; i < map.edgeCount; i++) {
                    Vector2 s = map.nodes[map.edges[i].startNode].position;
                    if (IsVisible(editor.camera, s, 200)) {
                        Vector2 e = map.nodes[map.edges[i].endNode].position;
                        DrawLineEx(s, e, map.edges[i].width, LIGHTGRAY);
                    }
                }

                // 3. Buildings
                for (int i = 0; i < map.buildingCount; i++) {
                    if (map.buildings[i].pointCount > 0 && IsVisible(editor.camera, map.buildings[i].footprint[0], 100)) {
                        DrawLineStrip(map.buildings[i].footprint, map.buildings[i].pointCount, map.buildings[i].color);
                    }
                }

                // 4. BOUNDARIES
                for (int i = 0; i < boundaryCount; i++) {
                    DrawLineEx(boundaries[i].start, boundaries[i].end, 3.0f, RED);
                    DrawCircleV(boundaries[i].start, 3.0f, RED);
                    DrawCircleV(boundaries[i].end, 3.0f, RED);
                }
                if (editor.state == STATE_DRAWING_BORDER && editor.hasStartPoint) {
                    DrawLineEx(editor.lastBorderPoint, worldMouse, 2.0f, Fade(RED, 0.5f));
                }

                // 5. Locations
                for(int i=0; i<map.locationCount; i++) {
                    if((int)map.locations[i].type == LOC_DELETED) continue;
                    if (!IsVisible(editor.camera, map.locations[i].position, 50)) continue;

                    int typeIdx = (int)map.locations[i].type;
                    if (typeIdx < 0) typeIdx = 0;
                    if (typeIdx >= EDITOR_LOC_COUNT) typeIdx = 0; 
                    
                    Color c = editor.typeColors[typeIdx];
                    float r = 6.0f / editor.camera.zoom;
                    if(r < 3) r = 3; if(r > 15) r = 15;
                    
                    DrawCircleV(map.locations[i].position, r, c);
                    DrawCircleLines(map.locations[i].position.x, map.locations[i].position.y, r, BLACK);

                    if (editor.camera.zoom > 0.4f) {
                        int labelSize = (int)(20.0f * uiScale); 
                        if(labelSize > 40) labelSize = 40; if(labelSize < 10) labelSize = 10;
                        DrawText(map.locations[i].name, map.locations[i].position.x, map.locations[i].position.y - r*2, labelSize, BLACK);
                    }
                }

                if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                    DrawCircleV(editor.pendingPos, 12.0f / editor.camera.zoom, RED);
                }

            EndMode2D();

            // UI
            DrawRectangle(0, uiTop, scrW, uiHeight, LIGHTGRAY);
            DrawLine(0, uiTop, scrW, uiTop, GRAY);
            
            for(int i=0; i<EDITOR_LOC_COUNT; i++) {
                Rectangle btn = editor.typeButtons[i];
                bool isSelected = (editor.selectedType == i);
                DrawRectangleRec(btn, isSelected ? WHITE : editor.typeColors[i]);
                DrawRectangleLinesEx(btn, isSelected ? 3 : 1, BLACK);
                
                int fontSize = (int)(14 * uiScale);
                int txtW = MeasureText(editor.typeNames[i], fontSize);
                DrawText(editor.typeNames[i], btn.x + (btn.width - txtW)/2, btn.y + (btn.height - fontSize)/2, fontSize, isSelected ? BLACK : WHITE);
            }

            int infoSize = (int)(20 * uiScale);
            const char* modeText = "EDITING";
            Color modeColor = DARKGRAY;
            if (editor.state == STATE_DRAWING_BORDER) { modeText = "BORDER DRAWING (F2)"; modeColor = RED; }
            if (editor.state == STATE_DRAWING_ZONE) { modeText = "ZONE DRAWING (F3 - Water)"; modeColor = BLUE; }

            DrawText(TextFormat("Mode: %s (F1: View)", modeText), (int)(10*uiScale), (int)(10*uiScale), infoSize, modeColor);

            // [NEW] Saved Notification
            if (saveTimer > 0) {
                DrawRectangle(scrW/2 - 100, 50, 200, 40, Fade(GREEN, saveTimer > 1.0f ? 1.0f : saveTimer));
                DrawText("MAP SAVED!", scrW/2 - 50, 60, 20, WHITE);
            }

            // Input Popup
            if (editor.state == STATE_NAMING || editor.state == STATE_EDITING) {
                float mw = 400 * uiScale; 
                float mh = 120 * uiScale;
                float mx = (scrW - mw)/2; 
                float my = (uiTop - mh)/2; 
                
                DrawRectangle(mx, my, mw, mh, Fade(WHITE, 0.95f));
                DrawRectangleLines(mx, my, mw, mh, BLACK);
                
                int popupFont = (int)(24 * uiScale);
                DrawText(editor.pendingName, mx + 20*uiScale, my + 40*uiScale, popupFont, BLACK);
                
                int subFont = (int)(14 * uiScale);
                DrawText("Press ENTER", mx + 20*uiScale, my + 80*uiScale, subFont, GRAY);
            }

        EndDrawing();
    }

    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}

// --- STUB FUNCTIONS ---
#include "player.h"
void EnterDealership(Player *player) { }
void ShowPhoneNotification(const char *text, Color color) { }
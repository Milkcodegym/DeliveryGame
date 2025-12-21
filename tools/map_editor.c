#include "raylib.h"
// #include "raymath.h" <--- We removed this to fix your error
#include "map.h"
#include <stdio.h>
#include <string.h>

// --- HOW TO COMPILE: ---
// run the following command from the main project folder:
//gcc tools/map_editor.c src/map.c -o map_editor.exe -O2 -Wall -I src -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm





// --- CONFIGURATION ---
const float EDITOR_MAP_SCALE = 0.25f; 
const char* MAP_FILE = "resources/Maps/real_city.map";

// --- EDITOR STATE ---
typedef enum {
    STATE_VIEW,
    STATE_ADDING,
    STATE_NAMING
} EditorState;

typedef struct {
    Camera2D camera;
    EditorState state;
    
    // Current Selection
    LocationType selectedType;
    Vector2 pendingPos;
    char pendingName[64];
    int nameLen;
    
    // UI State
    Rectangle typeButtons[7];
    const char* typeNames[7];
    Color typeColors[7];
} EditorData;

// --- HELPER: SAVE TO FILE ---
void SaveLocationToFile(MapLocation loc) {
    FILE *f = fopen(MAP_FILE, "a"); // Append Mode
    if (f == NULL) {
        printf("ERROR: Could not open map file for saving!\n");
        return;
    }

    float rawX = loc.position.x / EDITOR_MAP_SCALE;
    float rawY = loc.position.y / EDITOR_MAP_SCALE;
    
    char safeName[64];
    strncpy(safeName, loc.name, 64);
    for(int i=0; i<64; i++) {
        if(safeName[i] == ' ') safeName[i] = '_';
        if(safeName[i] == '\0') break;
    }

    fprintf(f, "\nL %d %.2f %.2f %s", loc.type, rawX, rawY, safeName);
    fclose(f);
    printf("Saved location: %s at %.2f, %.2f\n", safeName, rawX, rawY);
}

// --- MAIN EDITOR ---
int main(void) {
    InitWindow(1280, 720, "Developer Map Editor");
    SetTargetFPS(60);

    // Ensure you compile with map.c
    GameMap map = LoadGameMap(MAP_FILE); 
    
    EditorData editor = {0};
    editor.camera.zoom = 1.0f;
    editor.camera.offset = (Vector2){640, 360};
    editor.state = STATE_VIEW;
    editor.selectedType = LOC_HOUSE;
    
    // Setup UI
    const char* names[] = {"House", "Gas", "Market", "Shop", "Hot Food", "Cold Food", "Coffee"};
    Color colors[] = {GRAY, ORANGE, GREEN, PURPLE, RED, BLUE, BROWN};
    
    for(int i=0; i<7; i++) {
        editor.typeNames[i] = names[i];
        editor.typeColors[i] = colors[i];
        editor.typeButtons[i] = (Rectangle){ 10 + i*110, 650, 100, 40 };
    }

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mouse, editor.camera);

        // --- CONTROLS ---
        
        // 1. PANNING (Manual Math Fix)
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 delta = GetMouseDelta();
            // Manual scaling
            float scale = -1.0f / editor.camera.zoom;
            // Manual addition
            editor.camera.target.x += delta.x * scale;
            editor.camera.target.y += delta.y * scale;
        }

        // 2. ZOOMING (Manual Math Fix)
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouseWorldBefore = GetScreenToWorld2D(mouse, editor.camera);
            
            editor.camera.zoom += wheel * 0.25f;
            if (editor.camera.zoom < 0.1f) editor.camera.zoom = 0.1f;
            
            Vector2 mouseWorldAfter = GetScreenToWorld2D(mouse, editor.camera);
            
            // Manual vector subtraction and addition
            editor.camera.target.x += (mouseWorldBefore.x - mouseWorldAfter.x);
            editor.camera.target.y += (mouseWorldBefore.y - mouseWorldAfter.y);
        }

        // --- STATE LOGIC ---
        if (editor.state == STATE_VIEW || editor.state == STATE_ADDING) {
            // Select Type
            for(int i=0; i<7; i++) {
                if (CheckCollisionPointRec(mouse, editor.typeButtons[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    editor.selectedType = (LocationType)i;
                    editor.state = STATE_ADDING;
                }
            }

            // Click to Place
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mouse.y < 600) { 
                 editor.pendingPos = worldMouse;
                 
                 if (editor.selectedType == LOC_HOUSE) {
                     MapLocation newLoc = { "Delivery_Target", editor.pendingPos, LOC_HOUSE, 0 };
                     SaveLocationToFile(newLoc);
                 } else {
                     editor.state = STATE_NAMING;
                     editor.nameLen = 0;
                     memset(editor.pendingName, 0, 64);
                 }
            }
        }
        else if (editor.state == STATE_NAMING) {
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
                MapLocation newLoc;
                strncpy(newLoc.name, editor.pendingName, 64);
                newLoc.position = editor.pendingPos;
                newLoc.type = editor.selectedType;
                newLoc.iconID = (int)editor.selectedType;
                
                SaveLocationToFile(newLoc);
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
                for (int i = 0; i < map.edgeCount; i++) {
                    Vector2 s = map.nodes[map.edges[i].startNode].position;
                    Vector2 e = map.nodes[map.edges[i].endNode].position;
                    DrawLineEx(s, e, map.edges[i].width, LIGHTGRAY);
                }
                for (int i = 0; i < map.buildingCount; i++) {
                    DrawTriangleFan(map.buildings[i].footprint, map.buildings[i].pointCount, map.buildings[i].color);
                }
                
                if (editor.state == STATE_ADDING || editor.state == STATE_VIEW) {
                     DrawCircleV(worldMouse, 1.0f, editor.typeColors[editor.selectedType]);
                     DrawCircleLines(worldMouse.x, worldMouse.y, 2.0f, BLACK);
                }
                
                if (editor.state == STATE_NAMING) {
                    DrawCircleV(editor.pendingPos, 2.0f, RED);
                    DrawCircleLines(editor.pendingPos.x, editor.pendingPos.y, 3.0f, BLACK);
                }

            EndMode2D();

            // UI DRAWING
            DrawRectangle(0, 600, 1280, 120, LIGHTGRAY);
            DrawLine(0, 600, 1280, 600, GRAY);
            
            for(int i=0; i<7; i++) {
                Rectangle btn = editor.typeButtons[i];
                bool isSelected = (editor.selectedType == i);
                DrawRectangleRec(btn, isSelected ? WHITE : editor.typeColors[i]);
                DrawRectangleLinesEx(btn, isSelected ? 3 : 1, BLACK);
                int txtW = MeasureText(editor.typeNames[i], 10);
                DrawText(editor.typeNames[i], btn.x + (btn.width - txtW)/2, btn.y + 15, 10, isSelected ? BLACK : WHITE);
            }
            
            DrawText("Right Click to Pan | Scroll to Zoom | Left Click to Place", 10, 610, 10, DARKGRAY);

            if (editor.state == STATE_NAMING) {
                DrawRectangle(400, 300, 480, 120, Fade(WHITE, 0.95f));
                DrawRectangleLines(400, 300, 480, 120, BLACK);
                DrawText("NAME THIS LOCATION:", 420, 320, 20, GRAY);
                DrawText(editor.pendingName, 420, 360, 30, BLACK);
                if ((GetTime()*2) - (int)(GetTime()*2) < 0.5f) {
                    DrawRectangle(420 + MeasureText(editor.pendingName, 30) + 2, 360, 2, 30, BLACK);
                }
                DrawText("Press ENTER to Save, ESC to Cancel", 420, 400, 10, DARKGRAY);
            }

        EndDrawing();
    }

    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
#include "start_menu.h"
#include <math.h> 
#include <stdio.h>

// --- SHARED DRAWING FUNCTION ---
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status) {
    // 1. Draw a dark background to cover whatever is behind (the frozen game)
    DrawRectangle(0, 0, screenWidth, screenHeight, GetColor(0x202020FF)); 

    // 2. Moving stripes (Visual feedback that it hasn't crashed)
    float time = GetTime();
    int scrollOffset = (int)(time * 100) % 80; 
    for (int i = -1; i < screenHeight / 60 + 1; i++) {
        DrawRectangle(screenWidth / 2 - 10, (i * 80) + scrollOffset, 20, 40, YELLOW);
    }

    // 3. The Bar
    int barWidth = 400;
    int barHeight = 30;
    int barX = (screenWidth - barWidth) / 2;
    int barY = screenHeight / 2;

    DrawText("LOADING CITY...", barX, barY - 40, 20, WHITE);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    
    // Clamp progress for safety
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;

    DrawRectangle(barX + 5, barY + 5, (int)((barWidth - 10) * progress), barHeight - 10, GREEN);
    DrawText(status, barX, barY + 40, 20, LIGHTGRAY);
}

// --- MAIN MENU LOOP ---
GameMap RunStartMenu(const char* mapFileName) {
    float time = 0.0f;
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int loadingState = 0; 
    GameMap map = {0}; 

    while (!WindowShouldClose()) {
        time += GetFrameTime();
        
        if (loadingState == 0 && IsKeyPressed(KEY_ENTER)) {
            loadingState = 1;
        }

        BeginDrawing();
            if (loadingState == 0) {
                // ... [Keep your existing Main Menu Title/Car drawing code here] ...
                 ClearBackground(GetColor(0x202020FF));
                 DrawText("DELIVERY GAME", screenWidth/2 - 150, screenHeight/3, 40, RED);
                 DrawText("PRESS ENTER", screenWidth/2 - 80, screenHeight/3 + 60, 20, GREEN);
            }
            else {
                // LOADING SCREEN
                float progress = 0.0f;
                const char* status = "Initializing...";

                if (loadingState == 1) { 
                    progress = 0.1f; 
                    status = "Parsing Map Data..."; 
                }
                else if (loadingState == 2) { 
                    // CAP AT 95% - The final 5% happens in the main loop
                    progress = 0.95f; 
                    status = "Building 3D Geometry..."; 
                }
                
                DrawLoadingInterface(screenWidth, screenHeight, progress, status);
            }
        EndDrawing();

        // LOGIC
        if (loadingState == 1) {
            map = LoadGameMap(mapFileName);
            loadingState = 2;
        }
        else if (loadingState == 2) {
            GenerateMapBatch(&map);
            // Done! Break immediately. Do NOT wait.
            // We want the game loop to start immediately.
            break; 
        }
    }
    return map;
}
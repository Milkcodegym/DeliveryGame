#include "start_menu.h"
#include <math.h> 
#include <stdio.h>

// --- SHARED DRAWING FUNCTION ---
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status) {
    // 1. Draw a dark background to cover whatever is behind
    DrawRectangle(0, 0, screenWidth, screenHeight, GetColor(0x202020FF)); 

    // 2. Moving stripes (Visual feedback)
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
    
    // Clamp progress
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;

    DrawRectangle(barX + 5, barY + 5, (int)((barWidth - 10) * progress), barHeight - 10, GREEN);
    DrawText(status, barX, barY + 40, 20, LIGHTGRAY);
}

// --- MAIN MENU LOOP ---
GameMap RunStartMenu(const char* mapFileName,int screenWidth,int screenHeight) {
    float time = 0.0f;
    int loadingState = 0; 
    GameMap map = {0}; 

    while (!WindowShouldClose()) {
        time += GetFrameTime();
        
        // Input to start game
        if (loadingState == 0 && IsKeyPressed(KEY_ENTER)) {
            loadingState = 1;
        }

        BeginDrawing();
            if (loadingState == 0) {
                // Main Menu
                ClearBackground(GetColor(0x202020FF));
                DrawText("DELIVERY GAME", screenWidth/2 - 150, screenHeight/3, 40, RED);
                DrawText("PRESS ENTER", screenWidth/2 - 80, screenHeight/3 + 60, 20, GREEN);
            }
            else {
                // LOADING SCREEN
                // Since "Batch Generation" is gone, we just show "Loading Map..."
                DrawLoadingInterface(screenWidth, screenHeight, 0.5f, "Loading Assets & Map Data...");
            }
        EndDrawing();

        // LOGIC
        if (loadingState == 1) {
            // This function now handles everything (Models, Textures, Map Data)
            map = LoadGameMap(mapFileName);
            
            // Loading is done, break the loop to start the game
            break; 
        }
    }
    return map;
}
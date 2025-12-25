#include "start_menu.h"
#include <math.h> // Required for sin() function

void startmenu() {
    // Basic animation variables
    float time = 0.0f;
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    while (!WindowShouldClose()) {
        time += GetFrameTime(); // Update time for animations

        // 1. INPUT
        if (IsKeyPressed(KEY_ENTER)) {
            break;
        }

        BeginDrawing();
            // --- A. DYNAMIC BACKGROUND (Moving Road Effect) ---
            ClearBackground(GetColor(0x202020FF)); // Dark Asphalt Gray
            
            // Draw moving road stripes
            int scrollOffset = (int)(time * 100) % 80; 
            for (int i = -1; i < screenHeight / 60 + 1; i++) {
                DrawRectangle(screenWidth / 2 - 10, (i * 80) + scrollOffset, 20, 40, YELLOW);
            }

            // --- B. THE TITLE (With Shadow) ---
            const char* title = "DELIVERY GAME";
            int titleSize = 60;
            // Calculate center position
            int titleWidth = MeasureText(title, titleSize); 
            int titleX = (screenWidth - titleWidth) / 2;
            int titleY = screenHeight / 3;

            // Draw Shadow (Offset by 5 pixels)
            DrawText(title, titleX + 5, titleY + 5, titleSize, BLACK);
            // Draw Main Text
            DrawText(title, titleX, titleY, titleSize, RED);

            // --- C. PULSING SUBTITLE ---
            const char* subtitle = "PRESS [ENTER] TO START";
            int subSize = 20;
            int subWidth = MeasureText(subtitle, subSize);
            int subX = (screenWidth - subWidth) / 2;
            int subY = titleY + 80;

            // Math trick: sin() goes from -1 to 1. We map it to Alpha (transparency)
            float alpha = (sinf(time * 3.0f) + 1.0f) / 2.0f; // Range 0.0 to 1.0
            Color pulseColor = Fade(GREEN, alpha); // Raylib's Fade() handles transparency
            
            DrawText(subtitle, subX, subY, subSize, pulseColor);

            // --- D. DECORATION ---
            // Draw a simple car box below text
            DrawRectangle(screenWidth/2 - 40, subY + 50, 80, 40, BLUE); // Car Body
            DrawRectangle(screenWidth/2 - 30, subY + 40, 60, 10, SKYBLUE); // Windshield
            DrawCircle(screenWidth/2 - 25, subY + 90, 10, BLACK); // Wheel
            DrawCircle(screenWidth/2 + 25, subY + 90, 10, BLACK); // Wheel

        EndDrawing();
    }
}
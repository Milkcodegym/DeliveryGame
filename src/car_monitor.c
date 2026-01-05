#include "car_monitor.h"
#include <stdio.h>

// Helper for toggle buttons
static void ToggleBtn(Rectangle rect, const char* label, bool *state, Vector2 mouse, bool click) {
    bool hover = CheckCollisionPointRec(mouse, rect);
    Color c = *state ? GREEN : DARKGRAY;
    if (hover) c = Fade(c, 0.8f);
    
    DrawRectangleRec(rect, c);
    DrawRectangleLinesEx(rect, 2, BLACK);
    
    DrawText(label, rect.x + 10, rect.y + 10, 18, WHITE);
    
    if (*state) {
        DrawCircle(rect.x + rect.width - 20, rect.y + rect.height/2, 5, WHITE);
    } else {
        DrawCircleLines(rect.x + rect.width - 20, rect.y + rect.height/2, 5, LIGHTGRAY);
    }

    if (hover && click) {
        *state = !(*state);
    }
}

void DrawCarMonitorApp(Player *player, Vector2 localMouse, bool click) {
    // Background
    DrawRectangle(0, 0, 280, 600, (Color){20, 20, 25, 255});
    
    // Header
    DrawText("MyCarMonitor", 20, 40, 30, SKYBLUE);
    DrawText("v2.3", 230, 50, 10, GRAY);
    DrawLine(20, 80, 260, 80, DARKGRAY);

    DrawText("PIN DASHBOARD STATS", 20, 100, 10, LIGHTGRAY);

    float startY = 120;
    float gap = 50;

    // Default Stats
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Speedometer", &player->pinSpeed, localMouse, click);
    startY += gap;
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Fuel Gauge", &player->pinFuel, localMouse, click);
    startY += gap;
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Debug Accel", &player->pinAccel, localMouse, click);
    startY += gap;

    // Unlockables
    if (player->unlockThermometer) {
        ToggleBtn((Rectangle){20, startY, 240, 40}, "Food Temp.", &player->pinThermometer, localMouse, click);
    } else {
        DrawRectangle(20, startY, 240, 40, Fade(BLACK, 0.3f));
        DrawRectangleLines(20, startY, 240, 40, DARKGRAY);
        DrawText("LOCKED (Thermometer)", 30, startY + 12, 16, GRAY);
    }
    startY += gap;

    if (player->unlockGForce) {
        ToggleBtn((Rectangle){20, startY, 240, 40}, "G-Force Meter", &player->pinGForce, localMouse, click);
    } else {
        DrawRectangle(20, startY, 240, 40, Fade(BLACK, 0.3f));
        DrawRectangleLines(20, startY, 240, 40, DARKGRAY);
        DrawText("LOCKED (G-Force)", 30, startY + 12, 16, GRAY);
    }
    
    // --- LIVE DIAGNOSTICS (Moved Up) ---
    // Previous Y was 450. Buttons end around 360.
    // Moving to 390 leaves ~120px buffer at the bottom for the home button.
    float bY = 390; 
    
    DrawLine(20, bY, 260, bY, DARKGRAY);
    DrawText("LIVE DIAGNOSTICS", 20, bY + 10, 10, YELLOW);
    
    // Stats
    DrawText(TextFormat("Top Speed: %.0f km/h", player->max_speed * 3.0f), 20, bY + 35, 16, WHITE);
    
    float zeroToHundred = (player->acceleration > 0) ? (10.0f / player->acceleration) : 99.9f;
    DrawText(TextFormat("0-100 Time: %.1f s", zeroToHundred), 20, bY + 55, 16, WHITE);
    
    DrawText(TextFormat("Fuel Capacity: %.0f L", player->maxFuel), 20, bY + 75, 16, WHITE);
    
    float range = player->fuel / 0.05f; 
    DrawText(TextFormat("Est. Range: %.0f m", range), 20, bY + 95, 16, LIGHTGRAY);
}
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
    
    // Draw Pin Icon (Simple Circle) if active
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
    DrawText("v1.0", 230, 50, 10, GRAY);
    DrawLine(20, 80, 260, 80, DARKGRAY);

    DrawText("PIN DASHBOARD STATS", 20, 100, 10, LIGHTGRAY);

    float startY = 120;
    float gap = 50;

    // Default Stats
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Speedometer", &player->pinSpeed, localMouse, click);
    startY += gap;
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Fuel Gauge", &player->pinFuel, localMouse, click);
    startY += gap;
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Acceleration Data", &player->pinAccel, localMouse, click);
    startY += gap;

    // Unlockables
    if (player->unlockThermometer) {
        ToggleBtn((Rectangle){20, startY, 240, 40}, "Food Temp.", &player->pinThermometer, localMouse, click);
        startY += gap;
    } else {
        DrawRectangle(20, startY, 240, 40, Fade(BLACK, 0.3f));
        DrawRectangleLines(20, startY, 240, 40, DARKGRAY);
        DrawText("LOCKED (Thermometer)", 30, startY + 12, 16, GRAY);
        startY += gap;
    }

    if (player->unlockGForce) {
        ToggleBtn((Rectangle){20, startY, 240, 40}, "G-Force Meter", &player->pinGForce, localMouse, click);
        startY += gap;
    } else {
        DrawRectangle(20, startY, 240, 40, Fade(BLACK, 0.3f));
        DrawRectangleLines(20, startY, 240, 40, DARKGRAY);
        DrawText("LOCKED (G-Force)", 30, startY + 12, 16, GRAY);
        startY += gap;
    }

    // Info Section at bottom
    float bY = 450;
    DrawLine(20, bY, 260, bY, DARKGRAY);
    DrawText("LIVE DIAGNOSTICS", 20, bY + 10, 10, YELLOW);
    
    DrawText(TextFormat("Top Speed: %.0f km/h", player->max_speed * 3.0f), 20, bY + 30, 16, WHITE);
    DrawText(TextFormat("0-60 Time: %.1fs", 6.0f / player->acceleration), 20, bY + 50, 16, WHITE);
}
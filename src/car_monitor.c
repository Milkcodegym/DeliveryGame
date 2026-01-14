/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 *
 * License: zlib/libpng
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Full license terms: see the LICENSE file.
 * -----------------------------------------------------------------------------
 */

#include "car_monitor.h"
#include <stdio.h>

/*
 * Description: Draws a toggle button and updates its state based on mouse interaction.
 * Parameters:
 * - rect: The rectangle defining the button's position and size.
 * - label: The text label to display on the button.
 * - state: Pointer to the boolean state variable to toggle.
 * - mouse: The current mouse position.
 * - click: Boolean indicating if the mouse button was pressed.
 * Returns: None.
 */
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

/*
 * Description: Renders the "Car Monitor" app interface, allowing the user to toggle HUD elements and view stats.
 * Parameters:
 * - player: Pointer to the Player structure containing car stats and pin states.
 * - localMouse: The mouse position relative to the phone screen.
 * - click: Boolean indicating if the mouse button was pressed this frame.
 * Returns: None.
 */
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

    // Unlockables
    ToggleBtn((Rectangle){20, startY, 240, 40}, "Food Temp.", &player->pinThermometer, localMouse, click);
    startY += gap;

    ToggleBtn((Rectangle){20, startY, 240, 40}, "G-Force Meter", &player->pinGForce, localMouse, click);
    
    // --- LIVE DIAGNOSTICS ---
    // Moving to 390 leaves space at the bottom for the home button.
    float bY = 390; 
    
    DrawLine(20, bY, 260, bY, DARKGRAY);
    DrawText("LIVE DIAGNOSTICS", 20, bY + 10, 10, YELLOW);
    
    // Stats Display
    DrawText(TextFormat("Top Speed: %.0f km/h", player->max_speed * 5.0f), 20, bY + 35, 16, WHITE);
    
    float zeroToHundred = (player->acceleration > 0) ? (10.0f / player->acceleration) : 99.9f;
    DrawText(TextFormat("0-100 Time: %.1f s", zeroToHundred), 20, bY + 55, 16, WHITE);
    
    DrawText(TextFormat("Fuel Capacity: %.0f L", player->maxFuel), 20, bY + 75, 16, WHITE);
    
    // Range Calculation
    float range = player->maxFuel / player->fuelConsumption * 2.0f;
    char rangeText[32];
    
    if (range >= 1000.0f) {
        snprintf(rangeText, 32, "%.1f km", range * 2 / 1000.0f);
    } else {
        snprintf(rangeText, 32, "%d m", (int)range * 2);
    }
    
    DrawText(TextFormat("Est. Range: %s", rangeText), 20, bY + 95, 16, LIGHTGRAY);
}
#include "mechanic.h"
#include "player.h"
#include <stdio.h>

// Helper for cleaner buttons with Disabled State
static bool MechButton(Rectangle rect, const char* text, Color color, Vector2 mouse, bool disabled) {
    bool hover = CheckCollisionPointRec(mouse, rect);
    
    // Visuals
    Color drawColor = disabled ? GRAY : color;
    if (hover && !disabled) drawColor = Fade(color, 0.8f);
    
    DrawRectangleRec(rect, drawColor);
    DrawRectangleLinesEx(rect, 2, disabled ? DARKGRAY : BLACK);
    
    // Centered Text scaling attempt
    int fontSize = (int)(rect.height * 0.5f);
    if (fontSize < 10) fontSize = 10;
    int txtW = MeasureText(text, fontSize);
    DrawText(text, rect.x + (rect.width - txtW)/2, rect.y + (rect.height - fontSize)/2, fontSize, disabled ? LIGHTGRAY : WHITE);
    
    if (disabled) return false;
    return (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

bool DrawMechanicWindow(Player *player, PhoneState *phone, bool isActive, int screenW, int screenH) {
    if (!isActive) return false;

    // Scale UI
    float scale = (float)screenH / 720.0f;
    float w = 700 * scale; // Made wider for repair column
    float h = 500 * scale;
    float x = (screenW - w) / 2;
    float y = (screenH - h) / 2;
    Vector2 mouse = GetMousePosition();

    // Dim Background
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.6f));
    
    // Window Body
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    // Header
    DrawRectangle(x, y, w, 50 * scale, DARKBLUE);
    DrawText("JOE'S MECHANIC SHOP", x + 15*scale, y + 15*scale, 24*scale, WHITE);
    
    // Money Display
    DrawText(TextFormat("Cash: $%.0f", player->money), x + w - 150*scale, y + 15*scale, 20*scale, GREEN);

    float startY = y + 70 * scale;
    float col1X = x + 20 * scale;
    float col2X = x + 360 * scale; // Second column

    // --- COLUMN 1: REPAIRS & TUNING ---
    DrawText("Vehicle Service", col1X, startY, 20*scale, BLACK);
    startY += 35 * scale;

    // 1. REPAIR
    float damage = 100.0f - player->health;
    int repairCost = (int)(damage * 2.0f); // $2 per HP point
    if (repairCost < 10 && damage > 0) repairCost = 10; // Min charge
    if (damage <= 0) repairCost = 0;

    DrawText(TextFormat("Health: %.0f%%", player->health), col1X, startY, 16*scale, (player->health < 50) ? RED : DARKGREEN);
    
    const char* repairLabel = (repairCost == 0) ? "No Repairs Needed" : TextFormat("Repair ($%d)", repairCost);
    bool canAffordRepair = (player->money >= repairCost);
    
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 45*scale}, repairLabel, RED, mouse, (repairCost == 0 || !canAffordRepair))) {
        AddMoney(player, "Car Repair", -repairCost);
        player->health = 100.0f;
    }
    
    startY += 80 * scale;

    // 2. ACCELERATION
    DrawText(TextFormat("Engine Tune (Accel: %.1f)", player->acceleration), col1X, startY, 16*scale, DARKGRAY);
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 40*scale}, "Upgrade ($200)", ORANGE, mouse, player->money < 200)) {
        AddMoney(player, "Engine Upgrade", -200);
        player->acceleration += 0.2f;
    }
    startY += 75 * scale;

    // 3. BRAKES
    DrawText(TextFormat("Brake Pads (Power: %.1f)", player->brake_power), col1X, startY, 16*scale, DARKGRAY);
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 40*scale}, "Upgrade ($150)", ORANGE, mouse, player->money < 150)) {
        AddMoney(player, "Brake Upgrade", -150);
        player->brake_power += 1.0f;
    }
    
    // --- COLUMN 2: DIGITAL UPGRADES ---
    startY = y + 70 * scale;
    DrawText("MyCarMonitor App", col2X, startY, 20*scale, BLACK);
    startY += 35 * scale;

    if (!player->hasCarMonitorApp) {
        DrawText("Install the app to see", col2X, startY, 16*scale, GRAY);
        DrawText("live diagnostics.", col2X, startY+15*scale, 16*scale, GRAY);
        
        if (MechButton((Rectangle){col2X, startY + 40*scale, 280*scale, 50*scale}, "Buy App ($100)", BLUE, mouse, player->money < 100)) {
            AddMoney(player, "Bought App", -100);
            player->hasCarMonitorApp = true;
        }
    } else {
        DrawText("App Installed", col2X, startY, 16*scale, GREEN);
        startY += 30 * scale;

        // Sensor 1: Thermometer
        const char* label1 = player->unlockThermometer ? "Owned" : "Buy Thermo ($300)";
        Color c1 = player->unlockThermometer ? GRAY : PURPLE;
        bool disabledT = player->unlockThermometer || player->money < 300;
        
        DrawText("Food Thermometer", col2X, startY, 16*scale, DARKGRAY);
        if (MechButton((Rectangle){col2X, startY + 20*scale, 280*scale, 40*scale}, label1, c1, mouse, disabledT)) {
            AddMoney(player, "Thermometer", -300);
            player->unlockThermometer = true;
        }
        startY += 75 * scale;

        // Sensor 2: G-Force
        const char* label2 = player->unlockGForce ? "Owned" : "Buy G-Meter ($500)";
        Color c2 = player->unlockGForce ? GRAY : PURPLE;
        bool disabledG = player->unlockGForce || player->money < 500;

        DrawText("G-Force Sensor", col2X, startY, 16*scale, DARKGRAY);
        if (MechButton((Rectangle){col2X, startY + 20*scale, 280*scale, 40*scale}, label2, c2, mouse, disabledG)) {
            AddMoney(player, "G-Force Meter", -500);
            player->unlockGForce = true;
        }
    }

    // Close Button
    if (MechButton((Rectangle){x + w/2 - 60*scale, y + h - 50*scale, 120*scale, 40*scale}, "LEAVE", DARKGRAY, mouse, false)) {
        return false;
    }

    return true;
}
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

#include "mechanic.h"
#include <stdio.h>

// Helper for cleaner buttons with Disabled State
static bool MechButton(Rectangle rect, const char* text, Color color, Vector2 mouse, bool disabled) {
    bool hover = CheckCollisionPointRec(mouse, rect);
    
    Color drawColor = disabled ? GRAY : color;
    if (hover && !disabled) drawColor = Fade(color, 0.8f);
    
    DrawRectangleRec(rect, drawColor);
    DrawRectangleLinesEx(rect, 2, disabled ? DARKGRAY : BLACK);
    
    int fontSize = (int)(rect.height * 0.5f);
    if (fontSize < 10) fontSize = 10;
    int txtW = MeasureText(text, fontSize);
    DrawText(text, rect.x + (rect.width - txtW)/2, rect.y + (rect.height - fontSize)/2, fontSize, disabled ? LIGHTGRAY : WHITE);
    
    if (disabled) return false;
    return (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON));
}

bool DrawMechanicWindow(Player *player, PhoneState *phone, bool isActive, int screenW, int screenH) {
    if (!isActive) return false;

    float scale = (float)screenH / 720.0f;
    float w = 700 * scale; 
    float h = 600 * scale; // Increased height for more options
    float x = (screenW - w) / 2;
    float y = (screenH - h) / 2;
    Vector2 mouse = GetMousePosition();

    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.6f));
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    // Header
    DrawRectangle(x, y, w, 50 * scale, DARKBLUE);
    DrawText("JOE'S MECHANIC SHOP", x + 15*scale, y + 15*scale, 24*scale, WHITE);
    DrawText(TextFormat("Cash: $%.0f", player->money), x + w - 150*scale, y + 15*scale, 20*scale, GREEN);

    float startY = y + 70 * scale;
    float col1X = x + 20 * scale;
    float col2X = x + 360 * scale; 

    // --- COLUMN 1: PERFORMANCE ---
    DrawText("Performance", col1X, startY, 20*scale, BLACK);
    startY += 30 * scale;

    // 1. REPAIR
    float damage = 100.0f - player->health;
    int repairCost = (int)(damage * 2.0f); 
    if (repairCost < 10 && damage > 0) repairCost = 10; 
    if (damage <= 0) repairCost = 0;

    DrawText(TextFormat("Health: %.0f%%", player->health), col1X, startY, 16*scale, (player->health < 50) ? RED : DARKGREEN);
    const char* repairLabel = (repairCost == 0) ? "No Repairs Needed" : TextFormat("Repair ($%d)", repairCost);
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 40*scale}, repairLabel, RED, mouse, (repairCost == 0 || player->money < repairCost))) {
        AddMoney(player, "Car Repair", -repairCost);
        player->health = 100.0f;
    }
    startY += 70 * scale;

    // 2. ACCELERATION
    DrawText(TextFormat("Engine Tune (Accel: %.1f)", player->acceleration), col1X, startY, 16*scale, DARKGRAY);
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 40*scale}, "Upgrade ($200)", ORANGE, mouse, player->money < 200)) {
        AddMoney(player, "Engine Upgrade", -200);
        player->acceleration += 0.5f;
    }
    startY += 70 * scale;

    // 3. BRAKES
    DrawText(TextFormat("Brake Pads (Power: %.1f)", player->brake_power), col1X, startY, 16*scale, DARKGRAY);
    if (MechButton((Rectangle){col1X, startY + 20*scale, 280*scale, 40*scale}, "Upgrade ($150)", ORANGE, mouse, player->money < 150)) {
        AddMoney(player, "Brake Upgrade", -150);
        player->brake_power += 1.0f;
    }

    // --- COLUMN 2: UTILITY & TECH ---
    startY = y + 70 * scale; 
    DrawText("Utility & Tech", col2X, startY, 20*scale, BLACK);
    startY += 30 * scale;

    // 4. FUEL TANK
    DrawText(TextFormat("Fuel Tank (Max: %.0fL)", player->maxFuel), col2X, startY, 16*scale, DARKGRAY);
    if (MechButton((Rectangle){col2X, startY + 20*scale, 280*scale, 40*scale}, "Expand Tank ($350)", BLUE, mouse, player->money < 350)) {
        AddMoney(player, "Tank Expansion", -350);
        player->maxFuel += 10.0f;
    }
    startY += 70 * scale;

    // 5. THERMAL INSULATION [NEW]
    // Uses insulationFactor. 1.0 is default. Lower is better.
    // Display as "Quality %" where 1.0 is 0% quality and 0.0 is 100% quality for UI clarity.
    int insulationPct = (int)((1.0f - player->insulationFactor) * 100.0f);
    if (insulationPct < 0) insulationPct = 0;

    DrawText(TextFormat("Thermal Insulation (Qual: %d%%)", insulationPct), col2X, startY, 16*scale, DARKGRAY);
    
    bool maxedInsulation = (player->insulationFactor <= 0.2f); // Cap at 80% effectiveness
    const char* insLabel = maxedInsulation ? "Maxed Out" : "Add Lining ($400)";
    
    if (MechButton((Rectangle){col2X, startY + 20*scale, 280*scale, 40*scale}, insLabel, BLUE, mouse, (maxedInsulation || player->money < 400))) {
        AddMoney(player, "Insulation Upgrade", -400);
        player->insulationFactor *= 0.85f; // Improve by 15% exponentially
    }
    startY += 70 * scale;

    // 6. APPS
    /*if (!player->hasCarMonitorApp) {
        DrawText("MyCarMonitor App", col2X, startY, 16*scale, GRAY);
        if (MechButton((Rectangle){col2X, startY + 20*scale, 280*scale, 40*scale}, "Buy App ($100)", PURPLE, mouse, player->money < 100)) {
            AddMoney(player, "Bought App", -100);
            player->hasCarMonitorApp = true;
        }
    } else {
        // App Upgrades Mini-Section
        DrawText("Sensor Modules", col2X, startY, 16*scale, DARKGRAY);
        
        // Thermometer
        const char* labelT = player->unlockThermometer ? "Owned" : "Thermo ($300)";
        if (MechButton((Rectangle){col2X, startY + 20*scale, 135*scale, 40*scale}, labelT, PURPLE, mouse, (player->unlockThermometer || player->money < 300))) {
            AddMoney(player, "Thermometer", -300);
            player->unlockThermometer = true;
        }

        // G-Force
        const char* labelG = player->unlockGForce ? "Owned" : "G-Meter ($500)";
        if (MechButton((Rectangle){col2X + 145*scale, startY + 20*scale, 135*scale, 40*scale}, labelG, PURPLE, mouse, (player->unlockGForce || player->money < 500))) {
            AddMoney(player, "G-Force Meter", -500);
            player->unlockGForce = true;
        }
    }*/

    // Close Button
    if (MechButton((Rectangle){x + w/2 - 60*scale, y + h - 50*scale, 120*scale, 40*scale}, "LEAVE", DARKGRAY, mouse, false)) {
        return false;
    }

    return true;
}
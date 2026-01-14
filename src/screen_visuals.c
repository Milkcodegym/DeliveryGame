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

#include "screen_visuals.h"
#include <math.h>
#include <stdio.h>
#include "raymath.h" // Required for Vector3Lerp
#include "rlgl.h"    // Required for rlPushMatrix

// --- STATE FOR REFUEL SLIDER ---
static float targetFuelAmount = 0.0f;
static float fuelPricePerUnit = 1.50f; 
static float priceTimer = 0.0f;

// --- VISUAL EFFECTS SYSTEM ---
typedef struct {
    Vector3 startPos;
    Vector3 endPos;     // Where it's going
    float progress;     // 0.0 to 1.0
    bool active;
    bool isDropoff;     // New flag: True = Leaving Car, False = Entering Car
} DeliveryEffect;

static DeliveryEffect fxQueue[5] = {0};

// [NEW] Helper to find the active job (Picked Up)
static DeliveryTask* GetActiveTask(PhoneState *phone) {
    for (int i = 0; i < 5; i++) {
        if (phone->tasks[i].status == JOB_PICKED_UP) {
            return &phone->tasks[i];
        }
    }
    return NULL;
}

void UpdateVisuals(float dt) {
    priceTimer += dt;
    if (priceTimer >= 60.0f) { 
        priceTimer = 0.0f;
        float flux = (float)GetRandomValue(-5, 5) / 100.0f; 
        fuelPricePerUnit *= (1.0f + flux);
        if (fuelPricePerUnit < 0.50f) fuelPricePerUnit = 0.50f;
        if (fuelPricePerUnit > 5.00f) fuelPricePerUnit = 5.00f;
        fuelPricePerUnit = roundf(fuelPricePerUnit * 100.0f) / 100.0f;
    }
}

// Draws a BIGGER "Real" arrow shape with letters inside
void DrawRealArrow(int cx, int cy, int dir, bool isPressed) {
    Vector2 points[8] = {
        {0, -35},   {28, -7},   {11, -7},   {11, 35},
        {-11, 35},  {-11, -7},  {-28, -7},  {0, -18}
    };

    Vector2 finalPoints[8];
    Vector2 shadowPoints[8];
    int shadowOffset = 3;

    for (int i = 0; i < 8; i++) {
        float nx, ny;
        if (dir == 0)      { nx = points[i].x;  ny = points[i].y; }  
        else if (dir == 1) { nx = -points[i].y; ny = points[i].x; }  
        else if (dir == 2) { nx = -points[i].x; ny = -points[i].y; } 
        else               { nx = points[i].y;  ny = -points[i].x; } 
        
        finalPoints[i].x = cx + nx;
        finalPoints[i].y = cy + ny;
        shadowPoints[i].x = cx + nx + shadowOffset;
        shadowPoints[i].y = cy + ny + shadowOffset;
    }

    const char* textChar; 
    if (dir == 0) textChar = "W";
    else if (dir == 1) textChar = "D";
    else if (dir == 2) textChar = "S";
    else textChar = "A";
    
    int fontSize = 20;
    int textWidth = MeasureText(textChar, fontSize);
    int textOffsetX = textWidth / 2;
    int textOffsetY = fontSize / 2;

    float thickness = 3.0f;
    for (int i = 0; i < 7; i++) DrawLineEx(shadowPoints[i], shadowPoints[(i + 1) % 7], thickness, BLACK);
    DrawText(textChar, shadowPoints[7].x - textOffsetX, shadowPoints[7].y - textOffsetY, fontSize, BLACK);

    if (isPressed) {
        DrawTriangle(finalPoints[0], finalPoints[6], finalPoints[1], GRAY);
        DrawTriangle(finalPoints[5], finalPoints[4], finalPoints[2], GRAY);
        DrawTriangle(finalPoints[2], finalPoints[4], finalPoints[3], GRAY);
    }

    for (int i = 0; i < 7; i++) DrawLineEx(finalPoints[i], finalPoints[(i + 1) % 7], thickness, WHITE);
    DrawText(textChar, finalPoints[7].x - textOffsetX, finalPoints[7].y - textOffsetY, fontSize, WHITE);
}

void DrawWASDOverlay() {
    int spacing = 85;  
    int startX = 125;  
    int startY = GetScreenHeight() - 90; 
    DrawRealArrow(startX, startY - spacing, 0, IsKeyDown(KEY_W)); 
    DrawRealArrow(startX, startY, 2, IsKeyDown(KEY_S));           
    DrawRealArrow(startX - spacing, startY, 3, IsKeyDown(KEY_A)); 
    DrawRealArrow(startX + spacing, startY, 1, IsKeyDown(KEY_D)); 
}

// --- HELPER: Text Outline ---
void DrawTextOutline(const char *text, int posX, int posY, int fontSize, Color color, int spacing) {
    DrawText(text, posX - spacing, posY - spacing, fontSize, BLACK);
    DrawText(text, posX + spacing, posY - spacing, fontSize, BLACK);
    DrawText(text, posX - spacing, posY + spacing, fontSize, BLACK);
    DrawText(text, posX + spacing, posY + spacing, fontSize, BLACK);
    DrawText(text, posX, posY, fontSize, color);
}

// --- SPEEDOMETER ---
void DrawSpeedometer(float currentSpeed, float maxSpeed, int screenWidth) {
    // 1. Calculate Size
    float radius = screenWidth * 0.05f; 
    if (radius < 45) radius = 45; 

    // 2. Position: Shift LEFT (Increased multiplier to 1.3 to separate them)
    int centerX = (screenWidth / 2) - (int)(radius * 1.3f);
    int centerY = (int)(screenWidth * 0.06f); 

    // Font Settings
    int numSize = (int)(radius * 0.5f);  
    int labelSize = (int)(radius * 0.25f); 

    float needleLen = radius * 0.8f;
    if (currentSpeed < 0) currentSpeed = 0;
    if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;

    // Angles
    float startAngle = 180.0f; 
    float endAngle = 360.0f;
    float fraction = currentSpeed / maxSpeed;
    float needleAngle = startAngle + (fraction * 180.0f);

    // Background & Lines
    DrawCircleSector((Vector2){centerX, centerY}, radius, startAngle, endAngle, 30, Fade(BLACK, 0.6f));
    DrawCircleSectorLines((Vector2){centerX, centerY}, radius, startAngle, endAngle, 30, WHITE);
    DrawCircle(centerX, centerY, 5, WHITE);

    // Needle
    float rad = needleAngle * (PI / 180.0f);
    Vector2 needleEnd;
    needleEnd.x = centerX + cos(rad) * needleLen;
    needleEnd.y = centerY + sin(rad) * needleLen;
    DrawLineEx((Vector2){centerX, centerY}, needleEnd, 3.0f, RED);

    // Text with Outline
    char speedText[10];
    sprintf(speedText, "%0.0f", currentSpeed * 5);
    
    int textWidth = MeasureText(speedText, numSize);
    int labelWidth = MeasureText("KM/H", labelSize);

    DrawTextOutline(speedText, centerX - (textWidth / 2), centerY + 5, numSize, WHITE, 2);
    DrawTextOutline("KM/H", centerX - (labelWidth / 2), centerY + numSize + 5, labelSize, WHITE, 1);
}

// --- FUEL GAUGE ---
void DrawFuelOverlay(Player *player, int screenW, int screenH) {
    // 1. Size Logic
    float speedoRadius = screenW * 0.05f; 
    if (speedoRadius < 45) speedoRadius = 45;
    
    float gaugeRadius = speedoRadius * 0.7f; 

    // 2. Position: Shift RIGHT
    int centerX = (screenW / 2) + (int)(speedoRadius * 1.3f);
    int centerY = (int)(screenW * 0.06f);

    Vector2 center = { (float)centerX, (float)centerY }; 

    // Background
    DrawCircleV(center, gaugeRadius, Fade(BLACK, 0.6f));
    DrawCircleLines((int)center.x, (int)center.y, gaugeRadius, GRAY);
    
    // Labels (E and F)
    int fSize = (int)(gaugeRadius * 0.35f);
    DrawText("E", centerX - gaugeRadius + 5, centerY + 5, fSize, RED);
    DrawText("F", centerX + gaugeRadius - 15, centerY + 5, fSize, GREEN);

    // --- RANGE CALCULATION ---
    float consumption = player->fuelConsumption;
    if (consumption <= 0.001f) consumption = 0.01f; 
    
    // Approximation: 1 unit of fuel ~ 5 meters range? (Tuning required based on physics)
    float rangeMeters = (player->fuel / consumption) * 2.0f; // Multiplier tuned for visuals
    
    char rangeText[32];
    if (rangeMeters >= 1000.0f) {
        snprintf(rangeText, 32, "%.1f km", rangeMeters / 1000.0f);
    } else {
        snprintf(rangeText, 32, "%d m", (int)rangeMeters);
    }

    // Draw Range Text (Bottom Center)
    int rangeSize = (int)(gaugeRadius * 0.35f);
    int rangeWidth = MeasureText(rangeText, rangeSize);
    
    DrawText(rangeText, centerX - rangeWidth/2, centerY + (int)(gaugeRadius * 0.4f), rangeSize, WHITE);
    
    int lblSize = (int)(gaugeRadius * 0.2f);
    int lblWidth = MeasureText("REMAINING", lblSize);
    DrawText("REMAINING", centerX - lblWidth/2, centerY + (int)(gaugeRadius * 0.7f), lblSize, LIGHTGRAY);

    // Needle Logic
    float fuelPct = player->fuel / player->maxFuel;
    if (fuelPct < 0) fuelPct = 0; if (fuelPct > 1) fuelPct = 1;

    float startAngle = 210.0f; 
    float endAngle = 330.0f; 
    float currentAngle = startAngle + (endAngle - startAngle) * fuelPct;
    
    float rad = currentAngle * DEG2RAD;
    Vector2 needleEnd = {
        center.x + cosf(rad) * (gaugeRadius - 5),
        center.y + sinf(rad) * (gaugeRadius - 5)
    };
    
    DrawLineEx(center, needleEnd, 2.0f, RED);
    DrawCircleV(center, 3.0f, DARKGRAY);

    // Warning
    if (fuelPct < 0.2f) {
        if (((int)(GetTime() * 2) % 2) == 0) {
            const char* warnText = "LOW";
            int wSize = (int)(gaugeRadius * 0.5f);
            int txtW = MeasureText(warnText, wSize);
            DrawText(warnText, centerX - txtW/2, centerY - (int)(gaugeRadius * 0.3f), wSize, RED);
        }
    }
}

// [UPDATED] G-Force Visualizer with Real Fragility Limit
void DrawGForceMeter(Player *player, DeliveryTask *task, float x, float y, float scale) {
    float radius = 40.0f * scale;
    DrawCircle(x, y, radius, Fade(BLACK, 0.8f));
    DrawCircleLines(x, y, radius, WHITE);
    
    // Draw Safe Zone (Green) vs Danger Zone (Red) if cargo is fragile
    if (task && task->fragility > 0.0f) {
        // Must match delivery_app logic: gLimit = 1.5f * (1.0f - t->fragility);
        float gLimit = 1.5f * (1.0f - task->fragility); 
        if (gLimit < 0.3f) gLimit = 0.3f;
        
        // Scale limit to radius (Assuming 2.0G is edge of meter)
        float limitRadius = (gLimit / 2.0f) * radius; 
        if (limitRadius > radius) limitRadius = radius;

        DrawCircle(x, y, limitRadius, Fade(GREEN, 0.2f));
        DrawCircleLines(x, y, limitRadius, RED);
    } else {
        DrawCircleLines(x, y, radius * 0.5f, DARKGRAY);
    }

    DrawLine(x - radius, y, x + radius, y, DARKGRAY); // Horizontal Crosshair
    DrawLine(x, y - radius, x, y + radius, DARKGRAY); // Vertical Crosshair

    // Calculate approximate Gs
    // X axis = Turning (Lateral)
    float gX = 0.0f;
    if (IsKeyDown(KEY_A)) gX = 1.0f;
    if (IsKeyDown(KEY_D)) gX = -1.0f;
    // Scale by speed (turning faster = more Gs)
    gX *= (player->current_speed / player->max_speed) * 1.5f; 

    // Y axis = Accel/Brake (Longitudinal)
    // We cheat a bit and use IsKeyDown for instant visual feedback
    float gY = 0.0f;
    if (IsKeyDown(KEY_W)) gY = 0.5f; // Pulling back (accelerating)
    if (IsKeyDown(KEY_S)) gY = -0.8f; // Pushing forward (braking hard)
    // Adjust logic to match actual physics forces if available
    
    // Calculate magnitude to check against limit
    float magnitude = sqrtf(gX*gX + gY*gY);
    
    // Clamp visual dot to circle edge
    float visualMag = magnitude;
    if (visualMag > 2.0f) visualMag = 2.0f;
    float dist = (visualMag / 2.0f) * radius; // Map 0-2G to 0-Radius

    float angle = atan2f(gY, gX);
    float dotX = x + cosf(angle) * dist;
    float dotY = y + sinf(angle) * dist;

    Color dotColor = WHITE;
    // Check if violating limit
    if (task && task->fragility > 0.0f) {
        float gLimit = 1.5f * (1.0f - task->fragility);
        if (magnitude > gLimit) dotColor = RED;
    }

    DrawCircle(dotX, dotY, 6.0f * scale, dotColor);
    DrawText("G-FORCE", x - 20*scale, y + radius + 5*scale, 10*scale, BLACK);
}

// [UPDATED] Real Temperature Visualizer
void DrawThermometer(DeliveryTask *task, float x, float y, float scale) {
    float w = 20.0f * scale;
    float h = 80.0f * scale;
    
    // Background
    DrawRectangle(x, y, w, h, Fade(BLACK, 0.8f));
    DrawRectangleLines(x, y, w, h, WHITE);
    
    if (task && task->timeLimit > 0) {
        // Real Temperature Calculation
        // timeLimit is the total time allowed before "cold".
        // We compare GetTime() vs task->creationTime (which is reset on pickup)
        double elapsed = GetTime() - task->creationTime;
        float tempPct = 1.0f - ((float)elapsed / task->timeLimit);
        
        if (tempPct < 0.0f) tempPct = 0.0f;
        if (tempPct > 1.0f) tempPct = 1.0f;

        float fillH = h * tempPct;
        
        // Gradient Color
        Color tempColor = GREEN;
        if (tempPct < 0.5f) tempColor = ORANGE;
        if (tempPct < 0.2f) tempColor = RED;

        DrawRectangle(x + 2, y + h - fillH, w - 4, fillH, tempColor);
        DrawText("TEMP", x - 5*scale, y + h + 5*scale, 10*scale, tempColor);
    } else {
        // No Active Job or Non-Perishable
        DrawText("N/A", x + 2*scale, y + h/2 - 5*scale, 10*scale, GRAY);
        DrawText("TEMP", x - 5*scale, y + h + 5*scale, 10*scale, GRAY);
    }
    
    // Ticks
    DrawLine(x, y + h*0.2f, x+w, y + h*0.2f, GRAY); // High
    DrawLine(x, y + h*0.8f, x+w, y + h*0.8f, GRAY); // Low
}

// 1. Trigger SUCTION (Ground -> Car)
void TriggerPickupAnimation(Vector3 itemPos) {
    for (int i = 0; i < 5; i++) {
        if (!fxQueue[i].active) {
            fxQueue[i].active = true;
            fxQueue[i].isDropoff = false; // Suction
            fxQueue[i].startPos = itemPos;
            fxQueue[i].endPos = itemPos; // Will be updated to player position dynamically
            fxQueue[i].progress = 0.0f;
            return;
        }
    }
}

// 2. Trigger DROP-OFF (Car -> Ground) [NEW]
void TriggerDropoffAnimation(Vector3 playerPos, Vector3 targetGroundPos) {
    for (int i = 0; i < 5; i++) {
        if (!fxQueue[i].active) {
            fxQueue[i].active = true;
            fxQueue[i].isDropoff = true; // Drop off
            fxQueue[i].startPos = playerPos; // Start at car
            fxQueue[i].endPos = targetGroundPos; // End on ground
            fxQueue[i].progress = 0.0f;
            return;
        }
    }
}
// 3. Update & Draw Loop
#ifndef COLOR_CARDBOARD
#define COLOR_CARDBOARD (Color){ 170, 130, 100, 255 }
#define COLOR_TAPE      (Color){ 200, 180, 150, 255 }
#endif

void UpdateAndDrawPickupEffects(Vector3 playerPos) {
    float dt = GetFrameTime();
    float speed = 4.0f; 

    for (int i = 0; i < 5; i++) {
        if (!fxQueue[i].active) continue;

        fxQueue[i].progress += dt * speed;
        if (fxQueue[i].progress >= 1.0f) {
            fxQueue[i].active = false;
            continue;
        }

        // Calculate positions
        Vector3 start, end;
        if (fxQueue[i].isDropoff) {
            start = fxQueue[i].startPos;
            end = fxQueue[i].endPos;
        } else {
            start = fxQueue[i].startPos;
            end = playerPos; 
        }

        Vector3 current = Vector3Lerp(start, end, fxQueue[i].progress);

        // Scale Logic
        float scale = fxQueue[i].isDropoff ? fxQueue[i].progress : (1.0f - fxQueue[i].progress);
        
        // Prevent it from disappearing completely at the very end of pickup
        if (scale < 0.1f) scale = 0.1f; 

        // --- DRAW THE FLYING PACKAGE ---
        rlPushMatrix();
            rlTranslatef(current.x, current.y, current.z);
            
            // Spin effect
            rlRotatef(fxQueue[i].progress * 720.0f, 0.0f, 1.0f, 0.0f);
            // Tilt slightly to show the label
            rlRotatef(15.0f, 1.0f, 0.0f, 0.0f); 
            
            Vector3 center = {0, 0, 0};
            
            // Base dimensions of the package
            float baseW = 0.6f;
            float baseH = 0.4f;
            float baseD = 0.5f;

            // Scaled dimensions
            float w = baseW * scale;
            float h = baseH * scale;
            float d = baseD * scale;

            // 1. Main Box
            DrawCube(center, w, h, d, COLOR_CARDBOARD);
            DrawCubeWires(center, w, h, d, DARKBROWN);

            // 2. Label (White)
            // Offset slightly so it sits on top
            DrawCube((Vector3){0, h/2.0f + (0.01f*scale), 0}, w*0.7f, 0.01f*scale, d*0.7f, RAYWHITE);

            // 3. Tape (Strip)
            DrawCube(center, w + (0.02f*scale), h*0.15f, d + (0.02f*scale), COLOR_TAPE);

        rlPopMatrix();
        
        // --- DRAW THE TRAIL ---
        // We keep the color HERE so you know if it's a Pickup (Lime) or Delivery (Orange)
        Color trailColor = fxQueue[i].isDropoff ? ORANGE : LIME;
        DrawLine3D(start, current, Fade(trailColor, 0.5f));
    }
}
void DrawVisualsWithPinned(Player *player, PhoneState *phone) {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float scale = (float)screenH / 720.0f;

    // Fetch Active Task for "Real" Data
    DeliveryTask *activeTask = GetActiveTask(phone);

    // 1. WASD Overlay
    DrawWASDOverlay();

    // 2. Speedometer
    if (player->pinSpeed) {
        if (player->current_speed >= 0) DrawSpeedometer(player->current_speed, player->max_speed, screenW); 
        else DrawSpeedometer(-player->current_speed, player->max_speed, screenW);
    }

    // 3. Fuel Gauge
    if (player->pinFuel) {
        DrawFuelOverlay(player, screenW, screenH);
    }

    // 4. Pinned Gadgets
    float gadgetX = 60 * scale;
    float gadgetY = screenH - 250 * scale;
    float gap = 110 * scale;

    if (player->pinGForce) {
        DrawGForceMeter(player, activeTask, gadgetX, gadgetY, scale);
        gadgetY -= gap;
    }

    if (player->pinThermometer) {
        DrawThermometer(activeTask, gadgetX - 10*scale, gadgetY - 30*scale, scale); 
        gadgetY -= gap;
    }
    
}

// Add to screen_visuals.c or main.c

void DrawCargoHUD(PhoneState *phone, Player *player) {
    // 1. Find Active Job
    DeliveryTask *activeTask = NULL;
    for (int i = 0; i < 5; i++) {
        if (phone->tasks[i].status == JOB_PICKED_UP) {
            activeTask = &phone->tasks[i];
            break;
        }
    }
    if (!activeTask) return; // Nothing to show

    // 2. Setup HUD Panel
    float screenW = (float)GetScreenWidth();
    
    // Determine if we have a special condition (Fragile or Temp) to decide panel height
    bool isFragile = (activeTask->fragility > 0.0f);
    bool isTemp = (activeTask->timeLimit > 0 && (activeTask->jobType == LOC_FOOD || activeTask->jobType == LOC_CAFE));
    bool hasCondition = isFragile || isTemp;

    // Taller panel if we need to show two bars, shorter if just the timer
    float panelHeight = hasCondition ? 150.0f : 90.0f;
    Rectangle panel = { screenW - 270, 100, 250, panelHeight };

    DrawRectangleRounded(panel, 0.2f, 4, Fade(BLACK, 0.8f));
    DrawRectangleRoundedLines(panel, 0.2f, 4, DARKGRAY);

    // 3. Common Data Calculation
    double timeElapsed = GetTime() - activeTask->creationTime;
    float contentX = panel.x + 15;
    float currentY = panel.y + 10; // We will increment this to stack elements

    // ---------------------------------------------------------
    // SECTION A: TIMER (ALWAYS VISIBLE)
    // ---------------------------------------------------------
    DrawText("DELIVERY TIME", contentX, currentY, 16, WHITE);
    currentY += 25; // Move down for the bar/text

    if (activeTask->timeLimit > 0) {
        // --- Timer Bar ---
        float timePct = 1.0f - ((float)timeElapsed / activeTask->timeLimit);
        if (timePct < 0.0f) timePct = 0.0f;

        Rectangle timeBar = { contentX, currentY, 220, 25 };
        
        // Background
        DrawRectangleRec(timeBar, Fade(GRAY, 0.3f));
        // Fill
        DrawRectangle(timeBar.x, timeBar.y, timeBar.width * timePct, timeBar.height, (timePct > 0.3f) ? SKYBLUE : ORANGE);
        // Outline
        DrawRectangleLinesEx(timeBar, 2.0f, LIGHTGRAY);

        // Text: mm:ss
        int remaining = (int)(activeTask->timeLimit - timeElapsed);
        if (remaining < 0) remaining = 0;
        DrawText(TextFormat("%02d:%02d", remaining/60, remaining%60), timeBar.x + 85, timeBar.y + 4, 20, WHITE);
        
        currentY += 35; // Move Y down for the next section (if any)
    } else {
        // --- Standard Timer (No Limit) ---
        DrawText(TextFormat("%.1fs", (float)timeElapsed), contentX, currentY, 24, GREEN);
        currentY += 35; 
    }

    // ---------------------------------------------------------
    // SECTION B: SUB-CASES (FRAGILE OR TEMP)
    // ---------------------------------------------------------
    
    // --- SUBCASE 1: FRAGILE ---
    if (isFragile) {
        // Divider line (optional, purely aesthetic)
        DrawLine(panel.x + 10, currentY - 5, panel.x + panel.width - 10, currentY - 5, Fade(LIGHTGRAY, 0.3f));

        float healthPct = activeTask->pay / activeTask->maxPay;
        if (healthPct < 0.0f) healthPct = 0.0f;

        DrawText("CARGO INTEGRITY", contentX, currentY, 16, WHITE);
        
        // Health Bar
        Rectangle hpBar = { contentX, currentY + 25, 220, 25 };
        
        DrawRectangleRec(hpBar, Fade(RED, 0.3f)); // Background
        DrawRectangle(hpBar.x, hpBar.y, hpBar.width * healthPct, hpBar.height, (healthPct > 0.5f) ? LIME : RED);
        DrawRectangleLinesEx(hpBar, 2.0f, LIGHTGRAY);
        
        // Percentage Text
        DrawText(TextFormat("%d%%", (int)(healthPct * 100)), hpBar.x + 95, hpBar.y + 4, 20, WHITE);
        
        // Warning Icon
        DrawText("!", panel.x + 225, currentY, 20, ORANGE);
    }
    
    // --- SUBCASE 2: TEMPERATURE ---
    else if (isTemp) {
        // Divider line
        DrawLine(panel.x + 10, currentY - 5, panel.x + panel.width - 10, currentY - 5, Fade(LIGHTGRAY, 0.3f));

        double thermalTime = timeElapsed * player->insulationFactor;
        float tempPct = 1.0f - ((float)thermalTime / activeTask->timeLimit);
        if (tempPct < 0.0f) tempPct = 0.0f;

        DrawText("TEMPERATURE", contentX, currentY, 16, WHITE);

        // Temp Bar
        Rectangle tempBar = { contentX, currentY + 25, 220, 25 };
        
        DrawRectangleRec(tempBar, Fade(BLUE, 0.3f)); // Background
        
        // Gradient Logic
        Color tempColor = ORANGE;
        if (tempPct < 0.5f) tempColor = YELLOW;
        if (tempPct < 0.2f) tempColor = BLUE; // Too cold/spoiled

        DrawRectangle(tempBar.x, tempBar.y, tempBar.width * tempPct, tempBar.height, tempColor);
        DrawRectangleLinesEx(tempBar, 2.0f, LIGHTGRAY);

        // Insulation Text
        if (player->insulationFactor < 0.9f) {
            DrawText("Insulated", tempBar.x + 140, tempBar.y + 4, 16, Fade(WHITE, 0.7f));
        } else {
            DrawText("Cooling...", tempBar.x + 140, tempBar.y + 4, 16, Fade(RED, 0.7f));
        }
    }
}

bool DrawRefuelWindow(Player *player, bool isActive, int screenW, int screenH) {
    if (!isActive) return false;

    float maxAddable = player->maxFuel - player->fuel;
    if (targetFuelAmount > maxAddable) targetFuelAmount = maxAddable;
    if (targetFuelAmount < 0) targetFuelAmount = 0;
    
    float scale = (float)screenH / 720.0f;
    float w = 400 * scale; float h = 300 * scale;
    float x = (screenW - w) / 2; float y = (screenH - h) / 2;
    
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f)); 
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    DrawRectangle(x, y, w, 40 * scale, ORANGE);
    DrawText("GAS STATION", x + 10*scale, y + 10*scale, 20*scale, WHITE);
    
    float currentCost = targetFuelAmount * fuelPricePerUnit;
    
    float consumption = player->fuelConsumption; 
    if (consumption <= 0.0f) consumption = 0.01f; 
    float rangeAdded = (targetFuelAmount / consumption) * 2.0f;
    
    DrawText(TextFormat("Price: $%.2f / L", fuelPricePerUnit), x + 20*scale, y + 60*scale, 20*scale, DARKGRAY);
    DrawText(TextFormat("Your Cash: $%.0f", player->money), x + 20*scale, y + 90*scale, 20*scale, GREEN);
    DrawText("Fill Amount:", x + 20*scale, y + 130*scale, 20*scale, BLACK);
    
    Rectangle sliderBar = { x + 20*scale, y + 160*scale, w - 40*scale, 20*scale };
    float pct = (maxAddable > 0) ? (targetFuelAmount / maxAddable) : 0;
    Rectangle sliderKnob = { x + 20*scale + (pct * (sliderBar.width - 20*scale)), y + 155*scale, 20*scale, 30*scale };
    
    DrawRectangleRec(sliderBar, LIGHTGRAY);
    DrawRectangleRec(sliderKnob, DARKGRAY);
    
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Rectangle touchArea = { sliderBar.x, sliderBar.y - 10*scale, sliderBar.width, sliderBar.height + 20*scale };
        if (CheckCollisionPointRec(mouse, touchArea)) {
            float relativeX = mouse.x - sliderBar.x;
            float newPct = relativeX / sliderBar.width;
            if (newPct < 0) newPct = 0; if (newPct > 1) newPct = 1;
            targetFuelAmount = newPct * maxAddable;
            if (targetFuelAmount * fuelPricePerUnit > player->money) {
                targetFuelAmount = player->money / fuelPricePerUnit;
            }
        }
    }
    DrawText(TextFormat("+ %.1f L", targetFuelAmount), x + 20*scale, y + 190*scale, 20*scale, BLUE);
    DrawText(TextFormat("Cost: $%.2f", currentCost), x + 200*scale, y + 190*scale, 20*scale, RED);
    DrawText(TextFormat("+ %.0f m range", rangeAdded), x + 20*scale, y + 215*scale, 18*scale, GRAY);

    Rectangle btnBuy = { x + 50*scale, y + 250*scale, 120*scale, 35*scale };
    Rectangle btnCancel = { x + 230*scale, y + 250*scale, 120*scale, 35*scale };
    
    Color buyColor = (currentCost <= player->money && currentCost > 0) ? GREEN : GRAY;
    DrawRectangleRec(btnBuy, buyColor);
    DrawText("FILL UP", btnBuy.x + 25*scale, btnBuy.y + 8*scale, 20*scale, WHITE);
    DrawRectangleRec(btnCancel, RED);
    DrawText("CANCEL", btnCancel.x + 20*scale, btnCancel.y + 8*scale, 20*scale, WHITE);
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, btnCancel)) return false; 
        if (CheckCollisionPointRec(mouse, btnBuy)) {
            if (currentCost <= player->money && currentCost > 0) {
                AddMoney(player, "Fuel Purchase", -currentCost);
                player->fuel += targetFuelAmount;
                targetFuelAmount = 0; 
                return false; 
            }
        }
    }
    return true; 
}
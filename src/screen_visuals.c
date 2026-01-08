#include "screen_visuals.h"
#include "player.h"
#include <math.h>
#include <stdio.h>

// --- STATE FOR REFUEL SLIDER ---
static float targetFuelAmount = 0.0f;
static float fuelPricePerUnit = 1.50f; 
static float priceTimer = 0.0f;

// [NEW] Helper to find th  e active job (Picked Up)
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

    const char* textChar; // Fixed const warning
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

void DrawSpeedometer(float currentSpeed, float maxSpeed) {
    int centerX = GetScreenWidth() / 2;
    int centerY = 80;
    float radius = 60.0f;
    float needleLen = 50.0f;
    int shadowOffset = 2;

    if (currentSpeed < 0) currentSpeed = 0;
    if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;

    float startAngle = 180.0f; 
    float endAngle = 360.0f;
    float fraction = currentSpeed / maxSpeed;
    float needleAngle = startAngle + (fraction * 180.0f);

    DrawCircleSectorLines((Vector2){centerX + shadowOffset, centerY + shadowOffset}, radius, startAngle, endAngle, 30, BLACK);
    char speedText[10];
    sprintf(speedText, "%0.0f", currentSpeed*9);
    int textWidth = MeasureText(speedText, 40);
    DrawText(speedText, centerX - (textWidth / 2) + shadowOffset, centerY + 10 + shadowOffset, 40, BLACK);
    DrawText("KM/H", centerX - 20 + shadowOffset, centerY + 50 + shadowOffset, 10, BLACK);

    DrawCircleSectorLines((Vector2){centerX, centerY}, radius, startAngle, endAngle, 30, WHITE);
    DrawCircle(centerX, centerY, 5, WHITE);

    float rad = needleAngle * (PI / 180.0f);
    Vector2 needleEnd;
    needleEnd.x = centerX + cos(rad) * needleLen;
    needleEnd.y = centerY + sin(rad) * needleLen;
    DrawLineEx((Vector2){centerX, centerY}, needleEnd, 3.0f, RED);

    DrawText(speedText, centerX - (textWidth / 2), centerY + 10, 40, WHITE);
    DrawText("KM/H", centerX - 20, centerY + 50, 10, GRAY);
}

void DrawFuelOverlay(Player *player, int screenW, int screenH) {
    float scale = (float)screenH / 720.0f;
    float gaugeRadius = 60.0f * scale;
    Vector2 center = { screenW - gaugeRadius - 30 * scale, screenH - gaugeRadius - 30 * scale };
    
    DrawCircleV(center, gaugeRadius, Fade(BLACK, 0.8f));
    DrawCircleLines(center.x, center.y, gaugeRadius, GRAY);
    
    DrawText("E", center.x - gaugeRadius + 15*scale, center.y + 10*scale, 20*scale, RED);
    DrawText("F", center.x + gaugeRadius - 25*scale, center.y + 10*scale, 20*scale, GREEN);
    DrawText("FUEL", center.x - 20*scale, center.y + 25*scale, 10*scale, LIGHTGRAY);

    float fuelPct = player->fuel / player->maxFuel;
    float startAngle = 210.0f; 
    float endAngle = 330.0f; 
    float currentAngle = startAngle + (endAngle - startAngle) * fuelPct;
    
    float rad = currentAngle * DEG2RAD;
    Vector2 needleEnd = {
        center.x + cosf(rad) * (gaugeRadius - 10),
        center.y + sinf(rad) * (gaugeRadius - 10)
    };
    
    DrawLineEx(center, needleEnd, 3.0f * scale, RED);
    DrawCircleV(center, 5.0f * scale, DARKGRAY);

    if (fuelPct < 0.2f) {
        if (((int)(GetTime() * 2) % 2) == 0) {
            const char* warnText = "LOW FUEL";
            int fontSize = 30 * scale;
            int txtW = MeasureText(warnText, fontSize);
            DrawText(warnText, center.x - txtW/2, center.y - 30*scale, fontSize, RED);
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
        float gLimit = 1.5f * (1.0f - task->fragility); // Must match delivery_app logic
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
    float gX = 0.0f;
    if (IsKeyDown(KEY_A)) gX = -1.0f;
    if (IsKeyDown(KEY_D)) gX = 1.0f;
    gX *= (player->current_speed / player->max_speed); 

    float gY = 0.0f;
    if (player->acceleration > 0) gY = 1.0f; // Accelerating (pull back)
    if (player->brake_power > 0) gY = -1.0f; // Braking (push forward)

    // Calculate magnitude to check against limit
    float magnitude = sqrtf(gX*gX + gY*gY);
    // Visual multiplier (map 2G to radius edge)
    float visualMag = magnitude / 2.0f; 
    if (visualMag > 1.0f) visualMag = 1.0f;

    float dotX = x + (gX * (radius/2.0f)); // rough approximation for visual placement
    float dotY = y + (gY * (radius/2.0f));

    Color dotColor = WHITE;
    // Check if violating limit
    if (task && task->fragility > 0.0f) {
        float gLimit = 1.5f * (1.0f - task->fragility);
        if (magnitude > gLimit) dotColor = RED;
    }

    DrawCircle(dotX, dotY, 6.0f * scale, dotColor);
    DrawText("G-FORCE", x - 20*scale, y + radius + 5*scale, 10*scale, LIGHTGRAY);
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
        if (player->current_speed >= 0) DrawSpeedometer(player->current_speed, player->max_speed); 
        else DrawSpeedometer(-player->current_speed, player->max_speed);
    }

    // 3. Fuel Gauge
    if (player->pinFuel) {
        DrawFuelOverlay(player, screenW, screenH);
    }

    // 4. Pinned Gadgets
    float gadgetX = 60 * scale;
    float gadgetY = screenH - 250 * scale;
    float gap = 110 * scale;

    if (player->unlockGForce && player->pinGForce) {
        DrawGForceMeter(player, activeTask, gadgetX, gadgetY, scale);
        gadgetY -= gap;
    }

    if (player->unlockThermometer && player->pinThermometer) {
        DrawThermometer(activeTask, gadgetX - 10*scale, gadgetY, scale); 
        gadgetY -= gap;
    }
    
    // 5. Basic Accel Text (Debug)
    if (player->pinAccel) {
        DrawText(TextFormat("A: %.1f", player->acceleration), 20, 200, 20, GREEN);
    }
}

// ... [DrawRefuelWindow stays the same] ...
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
    
    // [FIX] Use player->fuelConsumption instead of undefined macro
    float consumption = player->fuelConsumption; 
    if (consumption <= 0.0f) consumption = 0.01f; // Safety
    float rangeAdded = targetFuelAmount / consumption;
    
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

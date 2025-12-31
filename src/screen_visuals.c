#include "screen_visuals.h"
#include <math.h>
#include <stdio.h>

// --- STATE FOR REFUEL SLIDER ---
static float targetFuelAmount = 0.0f;
static float fuelPricePerUnit = 1.50f; 
static float priceTimer = 0.0f; // [NEW] Timer for price flux

// [NEW] Updates dynamic elements like fuel price
void UpdateVisuals(float dt) {
    priceTimer += dt;
    if (priceTimer >= 60.0f) { // Every minute
        priceTimer = 0.0f;
        // Fluctuate +/- 5%
        float flux = (float)GetRandomValue(-5, 5) / 100.0f; 
        fuelPricePerUnit *= (1.0f + flux);
        
        // Clamp realistic bounds (e.g. $0.50 to $5.00)
        if (fuelPricePerUnit < 0.50f) fuelPricePerUnit = 0.50f;
        if (fuelPricePerUnit > 5.00f) fuelPricePerUnit = 5.00f;
        
        // Round to nearest hundredth
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

    char* textChar;
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
    
    // Analog Fuel Gauge (Bottom Right)
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
            
            float range = player->fuel / FUEL_CONSUMPTION_RATE; 
            const char* rangeText = TextFormat("Est: %.0fm", range);
            DrawText(rangeText, center.x - MeasureText(rangeText, 15*scale)/2, center.y - 50*scale, 15*scale, WHITE);
        }
    }
}

void DrawPinnedStats(Player *player) {
    if (!player->pinAccel && !player->pinThermometer && !player->pinGForce) return;

    int screenH = GetScreenHeight();
    float scale = (float)screenH / 720.0f;
    float x = 20 * scale;
    float y = screenH - 220 * scale; 
    float w = 180 * scale;
    
    DrawRectangleRounded((Rectangle){x, y, w, 150*scale}, 0.1f, 4, Fade(BLACK, 0.7f));
    DrawRectangleRoundedLines((Rectangle){x, y, w, 150*scale}, 0.1f, 4, WHITE);
    
    float textY = y + 10*scale;
    DrawText("CAR MONITOR", x + 10*scale, textY, 16*scale, SKYBLUE);
    textY += 25*scale;

    if (player->pinAccel) {
        DrawText(TextFormat("Accel: %.1f", player->acceleration), x + 10*scale, textY, 14*scale, WHITE);
        textY += 20*scale;
        DrawText(TextFormat("Brake: %.1f", player->brake_power), x + 10*scale, textY, 14*scale, WHITE);
        textY += 25*scale;
    }

    if (player->pinThermometer && player->unlockThermometer) {
        float temp = 20.0f + (fabs(player->current_speed) * 0.5f);
        DrawText(TextFormat("Temp: %.1f C", temp), x + 10*scale, textY, 14*scale, ORANGE);
        textY += 25*scale;
    }

    if (player->pinGForce && player->unlockGForce) {
        float g = 1.0f + (fabs(player->current_speed) * 0.05f); 
        DrawText(TextFormat("G-Force: %.2f g", g), x + 10*scale, textY, 14*scale, RED);
        textY += 25*scale;
    }
}

bool DrawRefuelWindow(Player *player, bool isActive, int screenW, int screenH) {
    if (!isActive) return false;

    float maxAddable = player->maxFuel - player->fuel;
    if (targetFuelAmount > maxAddable) targetFuelAmount = maxAddable;
    if (targetFuelAmount < 0) targetFuelAmount = 0;
    
    float scale = (float)screenH / 720.0f;
    float w = 400 * scale;
    float h = 300 * scale;
    float x = (screenW - w) / 2;
    float y = (screenH - h) / 2;
    
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f)); 
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    DrawRectangle(x, y, w, 40 * scale, ORANGE);
    DrawText("GAS STATION", x + 10*scale, y + 10*scale, 20*scale, WHITE);
    
    float currentCost = targetFuelAmount * fuelPricePerUnit;
    float rangeAdded = targetFuelAmount / FUEL_CONSUMPTION_RATE;
    
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
            if (newPct < 0) newPct = 0;
            if (newPct > 1) newPct = 1;
            targetFuelAmount = newPct * maxAddable;
            
            if (targetFuelAmount * fuelPricePerUnit > player->money) {
                targetFuelAmount = player->money / fuelPricePerUnit;
            }
        }
    }
    
    DrawText(TextFormat("+ %.1f L", targetFuelAmount), x + 20*scale, y + 190*scale, 20*scale, BLUE);
    DrawText(TextFormat("Cost: $%.2f", currentCost), x + 200*scale, y + 190*scale, 20*scale, RED);
    DrawText(TextFormat("+ %.0f m", rangeAdded), x + 20*scale, y + 215*scale, 18*scale, GRAY);

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

void DrawVisuals(float currentSpeed, float maxSpeed){
    DrawWASDOverlay();
    if (currentSpeed >= 0) DrawSpeedometer(currentSpeed, maxSpeed); 
}

void DrawVisualsWithPinned(Player *player) {
    DrawVisuals(player->current_speed, player->max_speed);
    DrawPinnedStats(player);
}
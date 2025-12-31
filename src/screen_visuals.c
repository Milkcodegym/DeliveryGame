#include "screen_visuals.h"
#include <math.h>
#include <stdio.h>

// --- STATE FOR REFUEL SLIDER ---
static float targetFuelAmount = 0.0f;
static float fuelPricePerUnit = 1.50f; // $1.50 per unit of fuel

// Draws a BIGGER "Real" arrow shape with letters inside
// dir: 0=Up(W), 1=Right(D), 2=Down(S), 3=Left(A)
void DrawRealArrow(int cx, int cy, int dir, bool isPressed) {
    // 1. Define Points (SCALED UP by approx 1.4x)
    Vector2 points[8] = {
        {0, -35},   // P0: Tip
        {28, -7},   // P1: Head Right Corner
        {11, -7},   // P2: Shaft Right Top
        {11, 35},   // P3: Shaft Right Bottom
        {-11, 35},  // P4: Shaft Left Bottom
        {-11, -7},  // P5: Shaft Left Top
        {-28, -7},  // P6: Head Left Corner
        {0, -18}    // P7: TEXT CENTER POINT
    };

    // 2. Rotate Points & Create Shadow Points
    Vector2 finalPoints[8];
    Vector2 shadowPoints[8];
    int shadowOffset = 3;
    int numPoints = 8; 

    for (int i = 0; i < numPoints; i++) {
        float nx, ny;
        if (dir == 0)      { nx = points[i].x;  ny = points[i].y; }  // Up
        else if (dir == 1) { nx = -points[i].y; ny = points[i].x; }  // Right
        else if (dir == 2) { nx = -points[i].x; ny = -points[i].y; } // Down
        else               { nx = points[i].y;  ny = -points[i].x; } // Left
        
        finalPoints[i].x = cx + nx;
        finalPoints[i].y = cy + ny;

        shadowPoints[i].x = cx + nx + shadowOffset;
        shadowPoints[i].y = cy + ny + shadowOffset;
    }

    // --- PREPARE TEXT ---
    char* textChar;
    if (dir == 0) textChar = "W";
    else if (dir == 1) textChar = "D";
    else if (dir == 2) textChar = "S";
    else textChar = "A";
    
    int fontSize = 20;
    int textWidth = MeasureText(textChar, fontSize);
    int textOffsetX = textWidth / 2;
    int textOffsetY = fontSize / 2;

    // --- DRAWING LAYERS ---
    float thickness = 3.0f;
    for (int i = 0; i < 7; i++) {
        DrawLineEx(shadowPoints[i], shadowPoints[(i + 1) % 7], thickness, BLACK);
    }
    DrawText(textChar, shadowPoints[7].x - textOffsetX, shadowPoints[7].y - textOffsetY, fontSize, BLACK);

    if (isPressed) {
        DrawTriangle(finalPoints[0], finalPoints[6], finalPoints[1], GRAY);
        DrawTriangle(finalPoints[5], finalPoints[4], finalPoints[2], GRAY);
        DrawTriangle(finalPoints[2], finalPoints[4], finalPoints[3], GRAY);
    }

    for (int i = 0; i < 7; i++) {
         DrawLineEx(finalPoints[i], finalPoints[(i + 1) % 7], thickness, WHITE);
    }
    DrawText(textChar, finalPoints[7].x - textOffsetX, finalPoints[7].y - textOffsetY, fontSize, WHITE);
}

void DrawWASDOverlay() {
    int spacing = 85;  
    int startX = 125;  
    int startY = GetScreenHeight() - 90; 

    DrawRealArrow(startX, startY - spacing, 0, IsKeyDown(KEY_W)); // Up
    DrawRealArrow(startX, startY, 2, IsKeyDown(KEY_S));           // Down
    DrawRealArrow(startX - spacing, startY, 3, IsKeyDown(KEY_A)); // Left
    DrawRealArrow(startX + spacing, startY, 1, IsKeyDown(KEY_D)); // Right
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

    // --- DRAW SHADOWS ---
    DrawCircleSectorLines((Vector2){centerX + shadowOffset, centerY + shadowOffset}, radius, startAngle, endAngle, 30, BLACK);
    char speedText[10];
    sprintf(speedText, "%0.0f", currentSpeed*9);
    int textWidth = MeasureText(speedText, 40);
    DrawText(speedText, centerX - (textWidth / 2) + shadowOffset, centerY + 10 + shadowOffset, 40, BLACK);
    DrawText("KM/H", centerX - 20 + shadowOffset, centerY + 50 + shadowOffset, 10, BLACK);

    // --- DRAW MAIN UI ---
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
    // Scaling factor (Baseline 720p)
    float scale = (float)screenH / 720.0f;
    
    // --- Analog Fuel Gauge (Bottom Right) ---
    float gaugeRadius = 60.0f * scale;
    Vector2 center = { screenW - gaugeRadius - 30 * scale, screenH - gaugeRadius - 30 * scale };
    
    // Background
    DrawCircleV(center, gaugeRadius, Fade(BLACK, 0.8f));
    DrawCircleLines(center.x, center.y, gaugeRadius, GRAY);
    
    // Markers
    DrawText("E", center.x - gaugeRadius + 15*scale, center.y + 10*scale, 20*scale, RED);
    DrawText("F", center.x + gaugeRadius - 25*scale, center.y + 10*scale, 20*scale, GREEN);
    DrawText("FUEL", center.x - 20*scale, center.y + 25*scale, 10*scale, LIGHTGRAY);

    // Needle Logic (E=210 deg, F=330 deg)
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

    // --- Low Fuel Warning ---
    if (fuelPct < 0.2f) {
        // Blink effect
        if (((int)(GetTime() * 2) % 2) == 0) {
            const char* warnText = "LOW FUEL";
            int fontSize = 30 * scale;
            int txtW = MeasureText(warnText, fontSize);
            DrawText(warnText, center.x - txtW/2, center.y - 30*scale, fontSize, RED);
            
            // Estimated range (units left / consumption rate)
            float range = player->fuel / FUEL_CONSUMPTION_RATE; 
            const char* rangeText = TextFormat("Est: %.0fm", range);
            DrawText(rangeText, center.x - MeasureText(rangeText, 15*scale)/2, center.y - 50*scale, 15*scale, WHITE);
        }
    }
}

bool DrawRefuelWindow(Player *player, bool isActive, int screenW, int screenH) {
    if (!isActive) return false;

    float maxAddable = player->maxFuel - player->fuel;
    if (targetFuelAmount > maxAddable) targetFuelAmount = maxAddable;
    if (targetFuelAmount < 0) targetFuelAmount = 0;
    
    // Scale UI
    float scale = (float)screenH / 720.0f;
    float w = 400 * scale;
    float h = 300 * scale;
    float x = (screenW - w) / 2;
    float y = (screenH - h) / 2;
    
    // Background
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f)); 
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    // Header
    DrawRectangle(x, y, w, 40 * scale, ORANGE);
    DrawText("GAS STATION", x + 10*scale, y + 10*scale, 20*scale, WHITE);
    
    // Content
    float currentCost = targetFuelAmount * fuelPricePerUnit;
    float rangeAdded = targetFuelAmount / FUEL_CONSUMPTION_RATE;
    
    DrawText(TextFormat("Price: $%.2f / L", fuelPricePerUnit), x + 20*scale, y + 60*scale, 20*scale, DARKGRAY);
    DrawText(TextFormat("Your Cash: $%.0f", player->money), x + 20*scale, y + 90*scale, 20*scale, GREEN);
    
    DrawText("Fill Amount:", x + 20*scale, y + 130*scale, 20*scale, BLACK);
    
    // --- SLIDER ---
    Rectangle sliderBar = { x + 20*scale, y + 160*scale, w - 40*scale, 20*scale };
    float pct = (maxAddable > 0) ? (targetFuelAmount / maxAddable) : 0;
    Rectangle sliderKnob = { x + 20*scale + (pct * (sliderBar.width - 20*scale)), y + 155*scale, 20*scale, 30*scale };
    
    DrawRectangleRec(sliderBar, LIGHTGRAY);
    DrawRectangleRec(sliderKnob, DARKGRAY);
    
    // Slider Interaction
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Rectangle touchArea = { sliderBar.x, sliderBar.y - 10*scale, sliderBar.width, sliderBar.height + 20*scale };
        if (CheckCollisionPointRec(mouse, touchArea)) {
            float relativeX = mouse.x - sliderBar.x;
            float newPct = relativeX / sliderBar.width;
            if (newPct < 0) newPct = 0;
            if (newPct > 1) newPct = 1;
            
            targetFuelAmount = newPct * maxAddable;
            
            // Cost Cap
            if (targetFuelAmount * fuelPricePerUnit > player->money) {
                targetFuelAmount = player->money / fuelPricePerUnit;
            }
        }
    }
    
    // Stats
    DrawText(TextFormat("+ %.1f L", targetFuelAmount), x + 20*scale, y + 190*scale, 20*scale, BLUE);
    DrawText(TextFormat("Cost: $%.2f", currentCost), x + 200*scale, y + 190*scale, 20*scale, RED);
    DrawText(TextFormat("+ %.0f m", rangeAdded), x + 20*scale, y + 215*scale, 18*scale, GRAY);

    // Buttons
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
    DrawSpeedometer(currentSpeed, maxSpeed);
}
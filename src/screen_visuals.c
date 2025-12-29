#include "screen_visuals.h"
#include <math.h>
#include <stdio.h>

// Draws a BIGGER "Real" arrow shape with letters inside
// dir: 0=Up(W), 1=Right(D), 2=Down(S), 3=Left(A)
void DrawRealArrow(int cx, int cy, int dir, bool isPressed) {
    // 1. Define Points (SCALED UP by approx 1.4x)
    // Points 0-6 define the arrow shape
    // Point 7 defines where the text center goes
    Vector2 points[8] = {
        {0, -35},   // P0: Tip
        {28, -7},   // P1: Head Right Corner
        {11, -7},   // P2: Shaft Right Top
        {11, 35},   // P3: Shaft Right Bottom
        {-11, 35},  // P4: Shaft Left Bottom
        {-11, -7},  // P5: Shaft Left Top
        {-28, -7},  // P6: Head Left Corner
        {0, -18}    // P7: TEXT CENTER POINT (roughly middle of the triangle head)
    };

    // 2. Rotate Points & Create Shadow Points
    Vector2 finalPoints[8];
    Vector2 shadowPoints[8];
    int shadowOffset = 3;
    int numPoints = 8; // Total points including text center

    for (int i = 0; i < numPoints; i++) {
        float nx, ny;
        // Rotation logic assuming base orientation is UP
        if (dir == 0)      { nx = points[i].x;  ny = points[i].y; }  // Up
        else if (dir == 1) { nx = -points[i].y; ny = points[i].x; }  // Right
        else if (dir == 2) { nx = -points[i].x; ny = -points[i].y; } // Down
        else               { nx = points[i].y;  ny = -points[i].x; } // Left
        
        // Translate to screen position
        finalPoints[i].x = cx + nx;
        finalPoints[i].y = cy + ny;

        // Create offset shadow coordinates
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

    // LAYER 1: ARROW SHADOW (Black Outline)
    float thickness = 3.0f;
    for (int i = 0; i < 7; i++) {
        // Note: We only loop to 7 here, as point 7 is just for text
        DrawLineEx(shadowPoints[i], shadowPoints[(i + 1) % 7], thickness, BLACK);
    }

    // LAYER 2: TEXT SHADOW (Black Text)
    DrawText(textChar, shadowPoints[7].x - textOffsetX, shadowPoints[7].y - textOffsetY, fontSize, BLACK);

    // LAYER 3: FILL (Grey if pressed)
    if (isPressed) {
        DrawTriangle(finalPoints[0], finalPoints[6], finalPoints[1], GRAY);
        DrawTriangle(finalPoints[5], finalPoints[4], finalPoints[2], GRAY);
        DrawTriangle(finalPoints[2], finalPoints[4], finalPoints[3], GRAY);
    }

    // LAYER 4: ARROW MAIN OUTLINE (White)
    for (int i = 0; i < 7; i++) {
         DrawLineEx(finalPoints[i], finalPoints[(i + 1) % 7], thickness, WHITE);
    }

    // LAYER 5: MAIN TEXT (White)
    // We draw white text even if pressed, for maximum contrast against grey fill
    DrawText(textChar, finalPoints[7].x - textOffsetX, finalPoints[7].y - textOffsetY, fontSize, WHITE);
}


void DrawWASDOverlay() {
    int spacing = 85;  
    int startX = 125;  
    int startY = GetScreenHeight() - 90; 

    // Draw the arrows (W, S, A, D)
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

    // --- DRAW SHADOWS (Black Layer) ---
    // 1. Gauge Shadow
    DrawCircleSectorLines((Vector2){centerX + shadowOffset, centerY + shadowOffset}, radius, startAngle, endAngle, 30, BLACK);
    // 2. Text Shadow (Draw text in black first)
    char speedText[10];
    sprintf(speedText, "%0.0f", currentSpeed*9);
    int textWidth = MeasureText(speedText, 40);
    DrawText(speedText, centerX - (textWidth / 2) + shadowOffset, centerY + 10 + shadowOffset, 40, BLACK);
    DrawText("KM/H", centerX - 20 + shadowOffset, centerY + 50 + shadowOffset, 10, BLACK);


    // --- DRAW MAIN UI (White/Color Layer) ---
    // 1. Gauge White
    DrawCircleSectorLines((Vector2){centerX, centerY}, radius, startAngle, endAngle, 30, WHITE);
    DrawCircle(centerX, centerY, 5, WHITE);

    // 2. Needle (Red)
    float rad = needleAngle * (PI / 180.0f);
    Vector2 needleEnd;
    needleEnd.x = centerX + cos(rad) * needleLen;
    needleEnd.y = centerY + sin(rad) * needleLen;
    DrawLineEx((Vector2){centerX, centerY}, needleEnd, 3.0f, RED);

    // 3. Text White
    DrawText(speedText, centerX - (textWidth / 2), centerY + 10, 40, WHITE);
    DrawText("KM/H", centerX - 20, centerY + 50, 10, GRAY);
}



void DrawVisuals(float currentSpeed, float maxSpeed){
    DrawWASDOverlay();
    DrawSpeedometer(currentSpeed, maxSpeed);
}
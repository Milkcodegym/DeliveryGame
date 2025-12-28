#include "screen_visuals.h"
#include <math.h>
#include <stdio.h>

void DrawRealArrow(int cx, int cy, int dir, bool isPressed) {
    // 1. Define Points
    Vector2 points[7] = {
        {0, -25}, {20, -5}, {8, -5}, {8, 25}, {-8, 25}, {-8, -5}, {-20, -5}
    };

    // 2. Rotate Points
    Vector2 finalPoints[7];
    Vector2 shadowPoints[7]; // Points for the shadow
    int shadowOffset = 3;    // How far the shadow is dropped

    for (int i = 0; i < 7; i++) {
        float nx, ny;
        if (dir == 0)      { nx = points[i].x;  ny = points[i].y; }  // Up
        else if (dir == 1) { nx = -points[i].y; ny = points[i].x; }  // Right
        else if (dir == 2) { nx = -points[i].x; ny = -points[i].y; } // Down
        else               { nx = points[i].y;  ny = -points[i].x; } // Left
        
        finalPoints[i].x = cx + nx;
        finalPoints[i].y = cy + ny;

        // Create shadow coordinates
        shadowPoints[i].x = cx + nx + shadowOffset;
        shadowPoints[i].y = cy + ny + shadowOffset;
    }

    // 3. DRAW SHADOW (The Outline in Black)
    // We draw this first so it sits behind everything
    float thickness = 3.0f;
    for (int i = 0; i < 7; i++) {
        Vector2 start = shadowPoints[i];
        Vector2 end = shadowPoints[(i + 1) % 7]; 
        DrawLineEx(start, end, thickness, BLACK); // Black Shadow
    }

    // 4. DRAW FILL (Background Layer if pressed)
    if (isPressed) {
        DrawTriangle(finalPoints[0], finalPoints[6], finalPoints[1], GRAY);
        DrawTriangle(finalPoints[5], finalPoints[4], finalPoints[2], GRAY);
        DrawTriangle(finalPoints[2], finalPoints[4], finalPoints[3], GRAY);
    }

    // 5. DRAW MAIN OUTLINE (The White Top Layer)
    for (int i = 0; i < 7; i++) {
        Vector2 start = finalPoints[i];
        Vector2 end = finalPoints[(i + 1) % 7]; 
        DrawLineEx(start, end, thickness, WHITE); // White Pop
    }
}

// Layout function
void DrawWASDOverlay() {
    int spacing = 60; 
    int startX = 100;
    int startY = GetScreenHeight() - 70;

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
    sprintf(speedText, "%0.0f", currentSpeed*8);
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
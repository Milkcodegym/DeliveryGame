#include "raylib.h"
#include <stdio.h> // Needed for sprintf

int main(void)
{
    InitWindow(800, 600, "City Generator - DEBUG MODE");

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 5.0f, 0.0f, 5.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load the image
    Image mapImage = LoadImage("resources/Maps/Testmap1.png"); 
    Color *mapPixels = LoadImageColors(mapImage);
    
    // DEBUG: Get the color of the very first pixel (0,0) to see what Raylib sees
    Color firstPixel = {0}; 
    if (mapImage.width > 0) firstPixel = mapPixels[0];

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);

                // If image loaded, draw the map
                if (mapImage.width > 0) {
                    for (int y = 0; y < mapImage.height; y++) {
                        for (int x = 0; x < mapImage.width; x++) {
                            Color c = mapPixels[y * mapImage.width + x];
                            Vector3 pos = { x * 1.0f, 0.0f, y * 1.0f };

                            // RELAXED COLOR CHECK (Fixes "slightly off" reds)
                            // We check if Red is HIGH (>200) and Blue/Green are LOW (<50)
                            if (c.r > 200 && c.g < 50 && c.b < 50) {
                                DrawCube((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, RED);
                                DrawCubeWires((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, MAROON);
                            }
                            // RELAXED BLACK CHECK
                            // We check if the pixel is NOT transparent (A>0) and very dark
                            else if (c.a > 0 && c.r < 50 && c.g < 50 && c.b < 50) {
                                DrawCube((Vector3){pos.x, 0.05f, pos.z}, 1.0f, 0.1f, 1.0f, DARKGRAY);
                            }
                        }
                    }
                }
            EndMode3D();

            // --- DEBUG OVERLAY ---
            DrawText("DEBUG INFO:", 10, 10, 20, DARKGRAY);
            
            // 1. Check if Image Loaded
            if (mapImage.width == 0) {
                DrawText("ERROR: map.png NOT FOUND!", 10, 40, 20, RED);
                DrawText("Make sure map.png is next to the .exe file", 10, 60, 20, RED);
            } else {
                char sizeText[50];
                sprintf(sizeText, "Map Size: %dx%d", mapImage.width, mapImage.height);
                DrawText(sizeText, 10, 40, 20, GREEN);

                // 2. Check Colors
                char colorText[100];
                sprintf(colorText, "Pixel(0,0) RGBA: %d, %d, %d, %d", 
                        firstPixel.r, firstPixel.g, firstPixel.b, firstPixel.a);
                DrawText(colorText, 10, 70, 20, BLACK);
                
                if (firstPixel.r < 200 && firstPixel.r > 0) {
                    DrawText("WARNING: Your 'Red' is not pure red!", 10, 95, 20, ORANGE);
                }
            }

        EndDrawing();
    }

    UnloadImageColors(mapPixels);
    UnloadImage(mapImage);
    CloseWindow();
    return 0;
}
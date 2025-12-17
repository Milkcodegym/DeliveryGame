#include "raylib.h"
#include <stdio.h> // Needed for sprintf
#include <math.h>

int main(void)
{
    InitWindow(800, 600, "City Generator");

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 5.0f, 0.0f, 5.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load the image
    Image mapImage = LoadImage("resources/Maps/Testmap2.png"); 
    Color *mapPixels = LoadImageColors(mapImage);
    
    // DEBUG: Get the color of the very first pixel (0,0) to see what Raylib sees
    Color firstPixel = {0}; 
    if (mapImage.width > 0) firstPixel = mapPixels[0];

    // --- PLAYER VARIABLES ---
    Vector3 playerPos = { 10.0f, 5.0f, 10.0f }; // Start in the air
    float playerSpeed = 6.0f;
    float playerRadius = 0.3f;
    float yVelocity = 0.0f;
    bool isGrounded = false;

    SetTargetFPS(60);
    // DisableCursor(); // Removed: Cursor not needed for this 3rd person view

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // 1. INPUT HANDLING
        Vector3 move = { 0 };
        if (IsKeyDown(KEY_W)) move.z -= 1.0f;
        if (IsKeyDown(KEY_S)) move.z += 1.0f;
        if (IsKeyDown(KEY_A)) move.x -= 1.0f;
        if (IsKeyDown(KEY_D)) move.x += 1.0f;

        // Normalize vector so diagonal isn't faster
        if (move.x != 0 || move.z != 0) {
            float length = sqrtf(move.x*move.x + move.z*move.z);
            move.x /= length; move.z /= length;
            move.x *= playerSpeed * dt;
            move.z *= playerSpeed * dt;
        }

        // 2. PHYSICS & GRAVITY
        yVelocity -= 20.0f * dt; // Gravity
        if (isGrounded && IsKeyPressed(KEY_SPACE)) yVelocity = 8.0f; // Jump
        playerPos.y += yVelocity * dt;

        // Floor Collision
        if (playerPos.y <= playerRadius) {
            playerPos.y = playerRadius;
            yVelocity = 0;
            isGrounded = true;
        } else {
            isGrounded = false;
        }

        // 3. BUILDING COLLISION (Grid based)
        if (mapImage.width > 0) {
            // Check X-Axis Movement
            int gx = (int)(playerPos.x + move.x + 0.5f);
            int gz = (int)(playerPos.z + 0.5f);
            bool hitWall = false;
            if (gx >= 0 && gx < mapImage.width && gz >= 0 && gz < mapImage.height) {
                Color c = mapPixels[gz * mapImage.width + gx];
                if (c.r > 200 && c.g < 50 && c.b < 50) hitWall = true;
            }
            if (!hitWall) playerPos.x += move.x;

            // Check Z-Axis Movement
            gx = (int)(playerPos.x + 0.5f);
            gz = (int)(playerPos.z + move.z + 0.5f);
            hitWall = false;
            if (gx >= 0 && gx < mapImage.width && gz >= 0 && gz < mapImage.height) {
                Color c = mapPixels[gz * mapImage.width + gx];
                if (c.r > 200 && c.g < 50 && c.b < 50) hitWall = true;
            }
            if (!hitWall) playerPos.z += move.z;
        }

        // 4. CAMERA FOLLOW
        camera.target = playerPos;
        camera.position = (Vector3){ playerPos.x, playerPos.y + 8.0f, playerPos.z + 8.0f };

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawSphere(playerPos, playerRadius, YELLOW);

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
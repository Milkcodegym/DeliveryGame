#include "raylib.h"
#include "map.h"
#include "player.h"
#include "traffic.h"
#include <stdio.h> // Needed for sprintf
#include <math.h>

int main(void)
{
    InitWindow(2000, 1500, "Delivery Game");

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initialize Subsystems
    GameMap map = LoadGameMap("resources/Maps/Testmap3.png");
    Player player = InitPlayer((Vector3){ 10.0f, 5.0f, 10.0f });
    DrawPlayer(&player);
    TrafficManager traffic = {0};
    InitTraffic(&traffic);

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        UpdatePlayer(&player, &map, dt);
        UpdateTraffic(&traffic, &player, &map, dt);

        // Camera Follow
        camera.target = player.position;
        camera.position = (Vector3){ player.position.x - 8.0f*sin(player.angle*DEG2RAD), player.position.y + 8.0f, player.position.z - 8.0f*cos(player.angle*DEG2RAD)};

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawGameMap(&map);
                //DrawPlayer(&player);
                DrawModelEx(player.model, player.position, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){1.1f, 1.1f, 1.1f}, WHITE);
                DrawTraffic(&traffic);
            EndMode3D();

            // --- DEBUG OVERLAY ---
            DrawText("DEBUG INFO:", 10, 10, 20, DARKGRAY);
            
            if (map.width == 0) {
                DrawText("ERROR: map.png NOT FOUND!", 10, 40, 20, RED);
            } else {
                char sizeText[50];
                sprintf(sizeText, "Map Size: %dx%d", map.width, map.height);
                DrawText(sizeText, 10, 40, 20, GREEN);
                
                // Optional: Draw Traffic Count
                DrawText("Traffic Active", 10, 70, 20, BLUE);
            }

        EndDrawing();
    }

    UnloadModel(player.model);
    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
#include "raylib.h"
#include "map.h"
#include "player.h"
#include <stdio.h> // Needed for sprintf

int main(void)
{
    InitWindow(800, 600, "Delivery Game");

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Initialize Subsystems
    GameMap map = LoadGameMap("resources/Maps/Testmap2.png");
    Player player = InitPlayer((Vector3){ 10.0f, 5.0f, 10.0f });

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        UpdatePlayer(&player, &map, dt);

        // Camera Follow
        camera.target = player.position;
        camera.position = (Vector3){ player.position.x, player.position.y + 8.0f, player.position.z + 8.0f };

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawGameMap(&map);
                DrawPlayer(&player);
            EndMode3D();

            // --- DEBUG OVERLAY ---
            DrawText("DEBUG INFO:", 10, 10, 20, DARKGRAY);
            
            if (map.width == 0) {
                DrawText("ERROR: map.png NOT FOUND!", 10, 40, 20, RED);
                DrawText("Make sure map.png is next to the .exe file", 10, 60, 20, RED);
            } else {
                char sizeText[50];
                sprintf(sizeText, "Map Size: %dx%d", map.width, map.height);
                DrawText(sizeText, 10, 40, 20, GREEN);
            }

        EndDrawing();
    }

    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
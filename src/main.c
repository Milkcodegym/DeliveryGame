#include "raylib.h"
#include "map.h"
#include "player.h"
#include "traffic.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    InitWindow(1600, 900, "Delivery Game - Real Map");

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // 1. Load Map
    GameMap map = LoadGameMap("resources/maps/real_city.map");
    
    // 2. Determine Safe Spawn Point
    // We grab the position of the first node in the map so we are definitely "in the city"
    Vector3 startPos = { 0.0f, 50.0f, 0.0f }; // Fallback
    if (map.nodeCount > 0) {
        startPos.x = map.nodes[0].position.x;
        startPos.z = map.nodes[0].position.y; // Map uses 2D x,y logic
        startPos.y = 30.0f; // Drop from 50 meters
    }

    // 3. Init Player
    Player player = InitPlayer(startPos);
    LoadPlayerContent(&player); // Use the renamed function

    TrafficManager traffic = {0};
    InitTraffic(&traffic);

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        UpdatePlayer(&player, &map, dt);
        UpdateTraffic(&traffic, &player, &map, dt);

        // Camera Follow Logic
        // Calculate the camera position behind the player based on angle
        float camDist = 3.0f;
        float camHeight = 1.5f;
        
        Vector3 desiredPos;
        desiredPos.x = player.position.x - camDist * sinf(player.angle * DEG2RAD);
        desiredPos.z = player.position.z - camDist * cosf(player.angle * DEG2RAD);
        desiredPos.y = player.position.y + camHeight;

        // Simple smooth follow (Lerp)
        camera.position.x += (desiredPos.x - camera.position.x) * 5.0f * dt;
        camera.position.z += (desiredPos.z - camera.position.z) * 5.0f * dt;
        camera.position.y += (desiredPos.y - camera.position.y) * 5.0f * dt;
        
        camera.target = player.position;

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(100, 1.0f); // Larger grid
                DrawGameMap(&map);
                
                // Draw Player Model
                // We offset Y slightly (-0.3) so wheels touch ground, not center of sphere
                Vector3 drawPos = player.position;
                drawPos.y -= 0.3f; 
                DrawModelEx(player.model, drawPos, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
                
                DrawTraffic(&traffic);
            EndMode3D();

            // Debug Stats
            DrawText(TextFormat("Pos: %.1f, %.1f", player.position.x, player.position.z), 10, 10, 20, BLACK);
            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 30, 20, DARKGRAY);

            if (map.nodeCount == 0) {
                DrawText("MAP FAILED TO LOAD", 10, 60, 20, RED);
            }

        EndDrawing();
    }

    UnloadModel(player.model);
    UnloadGameMap(&map);
    CloseWindow();
    return 0;
}
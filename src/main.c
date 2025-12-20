#include "raylib.h"
#include <stdio.h>
#include <math.h>

// Custom headers
#include "camera.h"
#include "map.h"
#include "player.h"
#include "traffic.h"
#include "phone.h" 
#include "maps_app.h"

int main(void)
{
    // Initialize Window
    InitWindow(1600, 900, "Delivery Game - v0.2");
    
    // IMPORTANT: Init Audio Device for the Music App
    InitAudioDevice(); 


    // --- Load Game Resources ---
    GameMap map = LoadGameMap("resources/maps/real_city.map");
    
    // [CRITICAL] Build the navigation graph for the GPS App
    // This connects all the roads so the GPS can calculate paths.
    if (map.nodeCount > 0) {
        BuildMapGraph(&map);
    }

    // Determine Safe Spawn Point
    Vector3 startPos = { 0.0f, 50.0f, 0.0f }; 
    if (map.nodeCount > 0) {
        // Spawn at the first node in the map file to avoid falling through void
        startPos.x = map.nodes[0].position.x;
        startPos.z = map.nodes[0].position.y; 
        startPos.y = 0.5f; // Slightly above ground
    }

    InitCamera();

    Player player = InitPlayer(startPos);
    LoadPlayerContent(&player); 

    TrafficManager traffic = {0};
    InitTraffic(&traffic);

    // Initialize Phone (starts Music app, Maps app, etc.)
    PhoneState phone = {0};
    InitPhone(&phone); 

    SetTargetFPS(60);

    // --- Main Game Loop ---
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Update Game Logic
        UpdatePlayer(&player, &map, dt);
        UpdateTraffic(&traffic, &player, &map, dt);
        
        // Update Phone
        // Now passing Player and Map so the GPS/Bank apps work correctly
        UpdatePhone(&phone, &player, &map); 

        Update_Camera(player.position, &map, player.angle, dt);

        // --- Drawing ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // 1. Draw 3D World
            BeginMode3D(camera);
                DrawGrid(100, 1.0f);
                DrawGameMap(&map);
                
                // Draw Player (Offset slightly so wheels touch ground properly)
                Vector3 drawPos = player.position;
                DrawModelEx(player.model, drawPos, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
                
                DrawTraffic(&traffic);
            EndMode3D();

            // 2. Draw 2D UI (Phone)
            // Passing Map and Player allows the GPS to render and the Bank to show money
            DrawPhone(&phone, &player, &map);
            
            // Tooltip
            if (!phone.isOpen) { 
                DrawText("Press TAB to open Phone", GetScreenWidth() - 250, GetScreenHeight() - 30, 20, DARKGRAY);
            }
            
            // Debug Stats
            DrawText(TextFormat("Pos: %.1f, %.1f", player.position.x, player.position.z), 10, 10, 20, BLACK);
            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 30, 20, DARKGRAY);

            if (map.nodeCount == 0) {
                DrawText("MAP FAILED TO LOAD", 10, 60, 20, RED);
            }

        EndDrawing();
    }

    // --- Cleanup ---
    UnloadModel(player.model);
    UnloadGameMap(&map);
    
    UnloadPhone(&phone); // Unload phone textures and music streams
    CloseAudioDevice();  // Close audio device
    
    CloseWindow();
    return 0;
}
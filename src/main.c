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
#include "delivery_app.h" 
#include "start_menu.h"

int main(void)
{
    // Initialize Window
    // Note: 2600x1900 is very large. Ensure your monitor supports this or use GetScreenWidth()
    InitWindow(2600, 1900, "Delivery Game - v0.3");
    
    // Move Audio Init OUTSIDE the loop to prevent re-initialization crashes on restart
    InitAudioDevice(); 

    while (!WindowShouldClose()){
        
        // 1. Run the Start Menu (Handles loading up to 100% of assets)
        // This function now returns a fully populated GameMap with assets loaded.
        GameMap map = RunStartMenu("resources/maps/real_city.map");
    
        // [CRITICAL] Build the navigation graph for the GPS App
        if (map.nodeCount > 0) { BuildMapGraph(&map); }

        // Determine Safe Spawn Point
        Vector3 startPos = {0, 0, 0};
        if (map.nodeCount > 0) {
            startPos = (Vector3){ map.nodes[0].position.x, 0.5f , map.nodes[0].position.y };
        }

        InitCamera(); // Ensure camera.h handles reset properly
        
        Player player = InitPlayer(startPos);
        LoadPlayerContent(&player); 

        TrafficManager traffic = {0};
        InitTraffic(&traffic);

        // Initialize Phone
        PhoneState phone = {0};
        InitPhone(&phone, &map); 

        SetTargetFPS(60);

        // --- TRANSITION VARIABLES ---
        // We render the game for a few frames while hiding it behind the loading screen.
        // This forces the GPU/Driver to compile shaders and upload geometry buffers 
        // without the user seeing the initial "stutter".
        int frameCounter = 0;
        const int WARMUP_FRAMES = 10; 

        // --- Main Game Loop ---
        while (player.health > 0 && !WindowShouldClose()){
            float dt = GetFrameTime();
            
            // 1. UPDATE PHASE
            // We only run game logic if we are done warming up
            if (frameCounter > WARMUP_FRAMES) {
                
                // Update Game Logic
                UpdatePlayer(&player, &map, &traffic, dt);
                UpdateTraffic(&traffic, player.position, &map, dt);
                
                // Update Map Visuals (Pulsing spheres at locations)
                UpdateMapEffects(&map, player.position);
                
                // Update Phone (GPS, Music, etc.)
                UpdatePhone(&phone, &player, &map); 

                Update_Camera(player.position, &map, player.angle, dt);
            }
            else {
                // OPTIONAL: During warmup, force camera to player pos so it doesn't drift
                // (Assuming Update_Camera handles initialization, otherwise set manually)
                // camera.target = player.position; 
            }

            // 2. DRAW PHASE
            BeginDrawing();
                ClearBackground(RAYWHITE);

                // A. Draw 3D World (ALWAYS DRAW THIS to warm up GPU)
                BeginMode3D(camera);
                    DrawGrid(100, 1.0f);
                    
                    // This is the heavy draw call we are hiding during warmup
                    DrawGameMap(&map, player.position);
                    
                    // Draw Player
                    Vector3 drawPos = player.position;
                    DrawModelEx(player.model, drawPos, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
                    
                    DrawTraffic(&traffic);
                EndMode3D();

                // B. UI & OVERLAY
                if (frameCounter <= WARMUP_FRAMES) {
                    // --- THE FAKE FRAME ---
                    // Draw the loading screen at 100% to hide the game rendering underneath
                    DrawLoadingInterface(GetScreenWidth(), GetScreenHeight(), 1.0f, "Finalizing...");
                    frameCounter++;
                }
                else {
                    // --- REAL GAME UI ---
                    // 1. Get the current mouse position
                    Vector2 mousePos = GetMousePosition();
                    bool isClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                    
                    // 2. Draw Phone
                    DrawPhone(&phone, &player, &map, mousePos, isClick);

                    // Tooltip
                    if (!phone.isOpen) { 
                        DrawText("Press TAB to open Phone", GetScreenWidth() - 273, GetScreenHeight() - 30, 20, DARKGRAY);
                    }
                    
                    DrawHealthBar(&player);

                    // Debug Stats
                    DrawText(TextFormat("Pos: %.1f, %.1f", player.position.x, player.position.z), 10, 10, 20, BLACK);
                    DrawText(TextFormat("FPS: %d", GetFPS()), 10, 30, 20, DARKGRAY);

                    if (map.nodeCount == 0) {
                        DrawText("MAP FAILED TO LOAD", 10, 60, 20, RED);
                    }
                }

            EndDrawing();
        }

        // --- Cleanup Resources (Per session) ---
        UnloadModel(player.model);
        UnloadGameMap(&map);
        UnloadPhone(&phone);
        // Note: Traffic might need unloading if it allocates memory
    }

    // --- Final Cleanup ---
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
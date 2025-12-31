#include "raylib.h"
#include "raymath.h" 
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
#include "screen_visuals.h"
#include "mechanic.h" 

// Forward declaration if not in map.h
void DrawZoneMarker(Vector3 pos, Color color);

int main(void)
{
    // Initialize Window
    InitWindow(1280, 720, "Delivery Game - v0.3");

    int monitorWidth = GetMonitorWidth(0);
    int monitorHeight = GetMonitorHeight(0);
    float screen_percent = 0.8;
    int screenWidth = screen_percent*monitorWidth;
    int screenHeight = screen_percent*monitorHeight;

    SetWindowSize(screenWidth, screenHeight);
    SetWindowPosition((monitorWidth - screenWidth) / 2,(monitorHeight - screenHeight) / 2);
    
    InitAudioDevice(); 

    while (!WindowShouldClose()){
        
        GameMap map = RunStartMenu("resources/maps/real_city.map",screenWidth,screenHeight);
    
        if (map.nodeCount > 0) { BuildMapGraph(&map); }

        Vector3 startPos = {0, 0, 0};
        if (map.nodeCount > 0) {
            startPos = (Vector3){ map.nodes[0].position.x, 0.5f , map.nodes[0].position.y };
        }

        InitCamera(); 
        
        Player player = InitPlayer(startPos);
        LoadPlayerContent(&player); 

        TrafficManager traffic = {0};
        InitTraffic(&traffic);

        PhoneState phone = {0};
        InitPhone(&phone, &map); 

        SetTargetFPS(60);

        int frameCounter = 0;
        const int WARMUP_FRAMES = 10; 
        
        // Interaction States
        bool isRefueling = false;
        bool isMechanicOpen = false;

        while (player.health > 0 && !WindowShouldClose()){
            float dt = GetFrameTime();
            
            // 1. UPDATE PHASE
            if (frameCounter > WARMUP_FRAMES) {
                
                UpdatePlayer(&player, &map, &traffic, dt);
                UpdateVisuals(dt); 
                
                Vector3 playerFwd = { -sinf(player.angle * DEG2RAD), 0.0f, -cosf(player.angle * DEG2RAD) };
                UpdateDevControls(&map, player.position, playerFwd);

                UpdateTraffic(&traffic, player.position, &map, dt);
                UpdateMapEffects(&map, player.position);
                UpdatePhone(&phone, &player, &map); 
                Update_Camera(player.position, &map, player.angle, dt);
                
                // Debug Trigger
                if (IsKeyPressed(KEY_F3)) {
                    isMechanicOpen = true;
                }

                // Check for Interactions
                if (!isRefueling && !isMechanicOpen && !phone.isOpen && fabs(player.current_speed) < 1.0f) {
                    
                    Vector2 playerP2 = { player.position.x, player.position.z };

                    for(int i=0; i<map.locationCount; i++) {
                        // Check Fuel
                        if (map.locations[i].type == LOC_FUEL) {
                            Vector2 pumpPos2 = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
                            // [CHANGED] Increased Radius to 12.0f
                            if (Vector2Distance(playerP2, pumpPos2) < 12.0f) { 
                                if (IsKeyPressed(KEY_E)) isRefueling = true;
                            }
                        }
                        // Check Mechanic
                        else if (map.locations[i].type == LOC_MECHANIC) {
                            Vector2 mechPos2 = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
                            // [CHANGED] Increased Radius to 12.0f
                            if (Vector2Distance(playerP2, mechPos2) < 12.0f) { 
                                if (IsKeyPressed(KEY_E)) isMechanicOpen = true;
                            }
                        }
                    }
                }
            }

            // 2. DRAW PHASE
            BeginDrawing();
                ClearBackground(RAYWHITE);

                BeginMode3D(camera);
                    DrawGrid(100, 1.0f);
                    DrawGameMap(&map, camera);
                    
                    for(int i=0; i<5; i++) {
                        DeliveryTask *t = &phone.tasks[i];
                        if (t->status == JOB_ACCEPTED) {
                            Vector3 pickupPos = { t->restaurantPos.x, 0.0f, t->restaurantPos.y };
                            DrawZoneMarker(pickupPos, LIME);
                        }
                        else if (t->status == JOB_PICKED_UP) {
                            Vector3 dropPos = { t->customerPos.x, 0.0f, t->customerPos.y };
                            DrawZoneMarker(dropPos, ORANGE);
                        }
                    }
                    
                    Vector3 drawPos = player.position;
                    DrawModelEx(player.model, drawPos, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
                    DrawTraffic(&traffic);
                    
                EndMode3D();
                
                DrawVisualsWithPinned(&player); 
                DrawFuelOverlay(&player, GetScreenWidth(), GetScreenHeight());

                if (frameCounter <= WARMUP_FRAMES) {
                    DrawLoadingInterface(GetScreenWidth(), GetScreenHeight(), 1.0f, "Finalizing...");
                    frameCounter++;
                }
                else {
                    Vector2 mousePos = GetMousePosition();
                    bool isClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                    
                    // UI STATE MACHINE
                    if (isRefueling) {
                        isRefueling = DrawRefuelWindow(&player, isRefueling, GetScreenWidth(), GetScreenHeight());
                    } 
                    else if (isMechanicOpen) {
                        isMechanicOpen = DrawMechanicWindow(&player, isMechanicOpen, GetScreenWidth(), GetScreenHeight());
                    }
                    else {
                        DrawPhone(&phone, &player, &map, mousePos, isClick);
                        
                        if (!phone.isOpen) { 
                            DrawText("Press TAB to open Phone", GetScreenWidth() - 273, GetScreenHeight() - 30, 20, DARKGRAY);
                        }
                    }
                    
                    DrawHealthBar(&player);

                    DrawText(TextFormat("Pos: %.1f, %.1f", player.position.x, player.position.z), 10, 10, 20, BLACK);
                    DrawText(TextFormat("FPS: %d", GetFPS()), 10, 30, 50, DARKGRAY);
                    DrawText("F1: Crash | F2: Roadwork | F3: Mechanic (Debug) | F4: Clear", 10, 80, 20, DARKGRAY);

                    if (map.nodeCount == 0) {
                        DrawText("MAP FAILED TO LOAD", 10, 60, 20, RED);
                    }
                }

            EndDrawing();
        }

        UnloadModel(player.model);
        UnloadGameMap(&map);
        UnloadPhone(&phone);
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
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
#include "save.h"
#include "dealership.h"
#include "tutorial.h"

void FixPath(void) {
    for (int i = 0; i < 5; i++) {
        if (DirectoryExists("resources")) {
            TraceLog(LOG_INFO, "PATH FIX: Found resources folder.");
            return;
        }
        ChangeDirectory("..");
    }
}

int main(void)
{
    FixPath();
    InitWindow(1280, 720, "Delivery Game - v0.5");

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
        
        // [TUTORIAL] Initialize
        InitTutorial();
        
        // [DEALERSHIP] Initialize
        InitDealership();

        // [SAVE SYSTEM] Attempt Auto-Load
        if (LoadGame(&player, &phone)) {
            printf("Save file loaded successfully.\n");
        } else {
            printf("Starting new game.\n");
        }

        SetTargetFPS(60);

        int frameCounter = 0;
        const int WARMUP_FRAMES = 10; 
        
        // Interaction States
        bool isRefueling = false;
        bool isMechanicOpen = false;
        bool isDead = false;
        float deathTimer = 0.0f; 
        Vector3 respawnPoint = startPos;

        while (!WindowShouldClose()){
            float dt = GetFrameTime();
            
            // --- 1. UPDATE PHASE ---
            bool lockInput = UpdateTutorial(&player, &phone, &map, dt, isRefueling, isMechanicOpen);

            // [DEALERSHIP CHECK]
            if (GetDealershipState() == DEALERSHIP_ACTIVE) {
                UpdateDealership(&player);
            }
            else if (frameCounter > WARMUP_FRAMES) {
                
                // --- DEATH CHECK ---
                if (!isDead && player.health <= 0) {
                    isDead = true;
                    deathTimer = 0.0f;
                    
                    float penalty = player.money * 0.40f;
                    player.money -= penalty;
                    AddMoney(&player, "Hospital Bills", -penalty); 

                    // Find Nearest Mechanic
                    float minDst = 999999.0f;
                    Vector2 pPos2 = { player.position.x, player.position.z };
                    respawnPoint = startPos; 

                    for(int i=0; i<map.locationCount; i++) {
                        if (map.locations[i].type == LOC_MECHANIC) {
                            Vector2 mechPos = map.locations[i].position;
                            float dst = Vector2Distance(pPos2, mechPos);
                            if (dst < minDst) {
                                minDst = dst;
                                respawnPoint = (Vector3){ mechPos.x, 0.5f, mechPos.y }; 
                            }
                        }
                    }
                    SaveGame(&player, &phone);
                }

                if (isDead) {
                    deathTimer += dt;
                    if (deathTimer > 3.0f && IsKeyPressed(KEY_ENTER)) {
                        player.position = respawnPoint;
                        player.health = 100.0f;
                        player.current_speed = 0.0f;
                        player.fuel = player.maxFuel;
                        isDead = false;
                        ResetMapCamera((Vector2){player.position.x, player.position.z});
                    }
                } 
                else {
                    // --- NORMAL GAME LOOP ---
                    
                    // [TUTORIAL LOCK] If tutorial window is open, stop car and traffic
                    if (lockInput) {
                        // Apply friction to stop car if a window popped up while driving
                        if (player.current_speed > 0) player.current_speed -= 10.0f * dt;
                        if (player.current_speed < 0) player.current_speed = 0;
                    } 
                    else {
                        // Allow driving
                        UpdatePlayer(&player, &map, &traffic, dt);
                        UpdateTraffic(&traffic, player.position, &map, dt);
                        UpdateDevControls(&map, &player);
                    }

                    UpdateVisuals(dt); 
                    UpdateMapEffects(&map, player.position);
                    UpdatePhone(&phone, &player, &map); 
                    Update_Camera(player.position, &map, player.angle, dt);
                    
                    if (IsKeyPressed(KEY_F3)) isMechanicOpen = true;

                    // --- INTERACTION LOGIC ---
                    if (!isRefueling && !isMechanicOpen && fabs(player.current_speed) < 5.0f) {
                        Vector2 playerPos2D = { player.position.x, player.position.z };
                        for(int i=0; i<map.locationCount; i++) {
                            Vector2 locPos2D = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
                            float dist = Vector2Distance(playerPos2D, locPos2D);

                            if (dist < 12.0f) {
                                if (map.locations[i].type == LOC_FUEL) {
                                    if (IsKeyPressed(KEY_E)) isRefueling = true;
                                }
                                else if (map.locations[i].type == LOC_MECHANIC) {
                                    if (IsKeyPressed(KEY_E)) isMechanicOpen = true;
                                }
                            }
                        }
                    }
                }
            }

            // --- 2. DRAW PHASE ---
            BeginDrawing();
                ClearBackground(RAYWHITE);

                if (GetDealershipState() == DEALERSHIP_ACTIVE) {
                    DrawDealership(&player);
                    // Draw Tutorial Overlay on top of Dealership if needed (e.g. "Buy a car")
                    DrawTutorial(&player, &phone);
                }
                else {
                    BeginMode3D(camera);
                        DrawGrid(100, 1.0f);
                        DrawGameMap(&map, camera);
                        UpdateRuntimeParks(&map, camera.position);
                        
                        // Draw Deliveries
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

                        // Draw Interaction Markers
                        if (!isDead && !isRefueling && !isMechanicOpen) {
                            Vector3 pPos3D = player.position;
                            for(int i=0; i<map.locationCount; i++) {
                                 Vector2 locPos2D = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
                                 Vector2 pPos2D = { pPos3D.x, pPos3D.z };
                                 
                                 if (Vector2Distance(pPos2D, locPos2D) < 144.0f) { 
                                     Vector3 labelPos = { locPos2D.x, 2.5f, locPos2D.y }; 
                                     if (map.locations[i].type == LOC_FUEL) DrawCube(labelPos, 0.5f, 0.5f, 0.5f, YELLOW); 
                                     else if (map.locations[i].type == LOC_MECHANIC) DrawCube(labelPos, 0.5f, 0.5f, 0.5f, BLUE); 
                                 }
                            }
                        }
                        
                        Vector3 drawPos = player.position;
                        DrawModelEx(player.model, drawPos, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){0.35f, 0.35f, 0.35f}, WHITE);
                        DrawTraffic(&traffic);
                    EndMode3D();
                    
                    // --- 2D UI LAYER ---
                    DrawVisualsWithPinned(&player,&phone); 
                    DrawFuelOverlay(&player, GetScreenWidth(), GetScreenHeight());
                    
                    // Interaction Text
                    if (!isDead && !isRefueling && !isMechanicOpen && fabs(player.current_speed) < 5.0f) {
                          Vector2 pPos2 = { player.position.x, player.position.z };
                          for(int i=0; i<map.locationCount; i++) {
                              Vector2 locPos2 = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
                              if (Vector2Distance(pPos2, locPos2) < 12.0f) {
                                  const char* txt = (map.locations[i].type == LOC_FUEL) ? "PRESS [E] TO REFUEL" : "PRESS [E] FOR MECHANIC";
                                  int txtW = MeasureText(txt, 20);
                                  DrawText(txt, GetScreenWidth()/2 - txtW/2, GetScreenHeight() - 100, 20, BLACK);
                                  DrawText(txt, GetScreenWidth()/2 - txtW/2 - 1, GetScreenHeight() - 100 - 1, 20, YELLOW);
                              }
                          }
                     }

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
                            isMechanicOpen = DrawMechanicWindow(&player, &phone, isMechanicOpen, GetScreenWidth(), GetScreenHeight());
                        }
                        else {
                            DrawPhone(&phone, &player, &map, mousePos, isClick);
                            if (!phone.isOpen) { 
                                DrawText("Press TAB to open Phone", GetScreenWidth() - 273, GetScreenHeight() - 30, 20, DARKGRAY);
                            }
                        }
                        DrawHealthBar(&player);

                        if (isDead) {
                            // ... Death Screen Drawing (Same as before) ...
                            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(MAROON, 0.8f));
                            DrawText("WASTED", GetScreenWidth()/2 - MeasureText("WASTED", 80)/2, GetScreenHeight()/3, 80, WHITE);
                            if (deathTimer > 3.0f) DrawText("Press [ENTER] to Respawn", GetScreenWidth()/2 - 180, GetScreenHeight()/2 + 60, 30, WHITE);
                        }

                        // Debug HUD
                        DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, BLACK);
                    }
                    
                    // [TUTORIAL OVERLAY] Drawn Last
                    DrawTutorial(&player, &phone);
                }
            EndDrawing();
        }

        if (player.health > 0) SaveGame(&player, &phone);
        
        UnloadModel(player.model);
        UnloadGameMap(&map);
        UnloadPhone(&phone);
        UnloadDealershipSystem(); 
    }
    
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 *
 * License: zlib/libpng
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Full license terms: see the LICENSE file.
 * -----------------------------------------------------------------------------
 */

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
    float borderMessageTimer = 0.0f; // Shows "Turn Back" message
    FixPath();
    SetTraceLogLevel(LOG_WARNING);
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
        
        if (!RunStartMenu_PreLoad(GetScreenWidth(), GetScreenHeight())) {
        CloseWindow();
        return 0; // User closed window
        }

        // Determine Map File
        const char* mapPath = "resources/maps/real_city.map"; // Default

        if (FileExists("map_config.dat")) {
            FILE *f = fopen("map_config.dat", "rb");
            if (f) {
                int choice = 1;
                fread(&choice, sizeof(int), 1, f);
                fclose(f);
                
                if (choice == 2) {
                    mapPath = "resources/maps/smaller_city.map"; // Big City
                    printf("MAIN: Loading Big City based on user choice.\n");
                } else {
                    printf("MAIN: Loading Small City based on user choice.\n");
                }
            }
        } else {
            // If no config but save exists, assume small city (legacy support)
            // Or just default to small city
            printf("MAIN: No config found, defaulting to Small City.\n");
        }

        // Load the chosen map
        GameMap map = LoadGameMap(mapPath);
        LoadMapBoundaries(mapPath); // Load corresponding boundaries

        Vector3 startPos = {0, 0, 0};
        if (map.nodeCount > 0) {
            startPos = (Vector3){ map.nodes[0].position.x, 0.5f , map.nodes[0].position.y };
        }

        bool isLoading = true;

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

            if (!lockInput && !isRefueling && !isMechanicOpen && !isDead) {
                UpdateDeliveryInteraction(&phone, &player, &map, dt);
            }

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
                        if (player.current_speed > 0) player.current_speed = player.current_speed * 0.4f;
                        if (player.current_speed < 0) player.current_speed = 0;
                        player.fuel = player.maxFuel;
                    } 
                    else {
                        // Allow driving
                        UpdatePlayer(&player, &map, &traffic, dt);
                        UpdateTraffic(&traffic, player.position, &map, dt);
                        UpdateDevControls(&map, &player);
                    }
                    // --- INVISIBLE BORDER CHECK ---
                    Vector3 pushVec;
                    if (CheckInvisibleBorder(player.position, 1.0f, &pushVec)) {
                        player.position = Vector3Add(player.position, pushVec);
                        player.current_speed = 0.0f; 
                        SetIgnorePhysics();
                        borderMessageTimer = 2.0f; 
                    }
                    UpdateMapStreaming(&map, player.position);
                    UpdateVisuals(dt); 
                    UpdateMapEffects(&map, player.position);
                    UpdatePhone(&phone, &player, &map); 
                    Update_Camera(player.position, &map, player.angle, dt);
                    
                    // [NEW] EMERGENCY FUEL LOGIC
                    if (player.fuel <= 0.0f) {
                        player.current_speed = Lerp(player.current_speed, 0.0f, 2.0f * dt); // Force stop
                        
                        if (IsKeyPressed(KEY_R)) {
                            float emergencyPricePerL = 4.50f; // 3x normal price (~1.50)
                            float emergencyAmount = 15.0f;    // Give 15 Liters
                            float totalCost = emergencyAmount * emergencyPricePerL;

                            // FAILSAFE: Bankruptcy Protection
                            if (player.money < 25.0f) {
                                player.fuel += 10.0f; // Charity fuel (smaller amount)
                                // Add notification logic here if you have a message system
                                printf("EMERGENCY: Charity fuel given (Player too poor)\n");
                            } 
                            // STANDARD: High Cost Rescue
                            else if (player.money >= totalCost) {
                                player.money -= totalCost;
                                player.fuel += emergencyAmount;
                                printf("EMERGENCY: Paid rescue service (-$%.2f)\n", totalCost);
                            }
                            // EDGE CASE: Has > 25 but < TotalCost (Partial Fill)
                            else {
                                float affordableFuel = player.money / emergencyPricePerL;
                                player.fuel += affordableFuel;
                                player.money = 0.0f;
                                printf("EMERGENCY: Partial rescue bought.\n");
                            }
                        }
                    }


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
                                
                                else if (map.locations[i].type == LOC_DEALERSHIP && IsKeyPressed(KEY_E)) {
                                    EnterDealership(&player);
                                }
                            }
                        }
                    }
                }
            }

            // --- 2. DRAW PHASE ---
            BeginDrawing();
                ClearBackground((Color){ 60, 150, 250, 255 });
                            

                
                if (GetDealershipState() == DEALERSHIP_ACTIVE) {
                    DrawDealership(&player);
                    // Draw Tutorial Overlay on top of Dealership if needed (e.g. "Buy a car")
                    DrawTutorial(&player, &phone);
                }
                else {
                    BeginMode3D(camera);
                        //DrawInvisibleBorders(); //for debugging border location
                        DrawGameMap(&map, camera);
                        UpdateRuntimeParks(&map, camera.position);
                        
                        // Draw Deliveries
                        for(int i = 0; i < 5; i++) {
                            DeliveryTask *t = &phone.tasks[i];

                            if (t->status == JOB_ACCEPTED) {
                                // 1. Get the raw center of the building
                                Vector3 rawPos = { t->restaurantPos.x, 0.0f, t->restaurantPos.y };
                                
                                // 2. Calculate the position on the street (OUTSIDE the building)
                                Vector3 smartPos = GetSmartDeliveryPos(&map, rawPos);
                                
                                // 3. Draw the marker at the smart position
                                DrawZoneMarker(&map, camera, smartPos, LIME);
                            }
                            else if (t->status == JOB_PICKED_UP) {
                                // Same logic for the customer drop-off
                                Vector3 rawPos = { t->customerPos.x, 0.0f, t->customerPos.y };
                                Vector3 smartPos = GetSmartDeliveryPos(&map, rawPos);
                                
                                DrawZoneMarker(&map, camera, smartPos, ORANGE);
                            }
                        }

                        UpdateAndDrawPickupEffects(player.position);

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
                        DrawModelEx(player.model, player.position, (Vector3){0.0f, 1.0f, 0.0f}, player.angle, (Vector3){0.35f, 0.35f, 0.35f}, WHITE);
                        DrawTraffic(&traffic);
                    EndMode3D();
                    
                    //DEBUG
                    // Paste this inside main.c, AFTER EndMode3D() but BEFORE EndDrawing()

                    // [DEBUG HUD] Paste this in main.c after DrawFuelOverlay
                    if (IsKeyDown(KEY_F1)) { // Hold F1 to see Debug Info
                        DrawRectangle(10, 10, 350, 160, Fade(BLACK, 0.7f));
                        DrawText(TextFormat("FPS: %d", GetFPS()), 20, 20, 20, GREEN);
                        
                        // Check Traffic Count
                        int activeCars = 0;
                        for(int i=0; i<MAX_VEHICLES; i++) if(traffic.vehicles[i].active) activeCars++;
                        DrawText(TextFormat("Active Cars: %d / %d", activeCars, MAX_VEHICLES), 20, 50, 20, activeCars > 0 ? GREEN : RED);

                        // Check Map Graph
                        if (map.graph) DrawText("Map Graph: CONNECTED", 20, 80, 20, GREEN);
                        else DrawText("Map Graph: MISSING!", 20, 80, 20, RED);

                        // Check Node Distance
                        int closest = GetClosestNode(&map, (Vector2){player.position.x, player.position.z});
                        if (closest != -1) {
                            float d = Vector2Distance((Vector2){player.position.x, player.position.z}, map.nodes[closest].position);
                            DrawText(TextFormat("Dst to Node: %.1f", d), 20, 110, 20, WHITE);
                        } else {
                            DrawText("Dst to Node: > 500m (Too Far)", 20, 110, 20, RED);
                        }
                        DrawText("F5: Force Spawn", 20, 140, 20, YELLOW);
                    }

                    // [DEBUG] Force Spawn Logic (F5)
                    if (IsKeyPressed(KEY_F5)) {
                        // 1. Find the next empty slot
                        int slot = -1;
                        for(int k=0; k<MAX_VEHICLES; k++) {
                            if (!traffic.vehicles[k].active) { slot = k; break; }
                        }

                        if (slot != -1 && map.nodeCount > 0) {
                            int closeNode = GetClosestNode(&map, (Vector2){player.position.x, player.position.z});
                            // If regular search fails, pick a random node just to prove cars work
                            if (closeNode == -1) closeNode = GetRandomValue(0, map.nodeCount-1);

                            int edgeIdx = FindNextEdge(&map, closeNode, -1);
                            
                            if (edgeIdx != -1) {
                                Vehicle *v = &traffic.vehicles[slot];
                                v->active = true;
                                v->currentEdgeIndex = edgeIdx;
                                v->speed = 0.0f; // Stopped so you can see it
                                v->startNodeID = map.edges[edgeIdx].startNode;
                                v->endNodeID = map.edges[edgeIdx].endNode;
                                v->progress = 0.5f; 
                                
                                Vector3 p1 = { map.nodes[v->startNodeID].position.x, 0, map.nodes[v->startNodeID].position.y };
                                Vector3 p2 = { map.nodes[v->endNodeID].position.x, 0, map.nodes[v->endNodeID].position.y };
                                v->edgeLength = Vector3Distance(p1, p2);
                                v->color = PURPLE;
                                v->position = player.position; // Spawn ON player to verify
                            }
                        }
                    }


                                        // Inside 2D Drawing Loop
                    if (borderMessageTimer > 0.0f) {
                        borderMessageTimer -= GetFrameTime();
                        
                        const char* text = "RESTRICTED AREA - TURN BACK";
                        int fontSize = 30;
                        int txtW = MeasureText(text, fontSize);
                        int screenW = GetScreenWidth();
                        int screenH = GetScreenHeight();
                        
                        // Draw Red Banner centered near top
                        DrawRectangle(screenW/2 - txtW/2 - 20, 100, txtW + 40, 50, Fade(RED, 0.8f));
                        DrawRectangleLines(screenW/2 - txtW/2 - 20, 100, txtW + 40, 50, BLACK);
                        DrawText(text, screenW/2 - txtW/2, 110, fontSize, WHITE);
                    }
                    // --- 2D UI LAYER ---
                    DrawVisualsWithPinned(&player,&phone); 

                    // [FIXED] Use functions to get data from delivery_app.c
                    if (IsInteractionActive()) {
                        int scrW = GetScreenWidth();
                        int scrH = GetScreenHeight();
                        float timer = GetInteractionTimer();
                        
                        if (timer > 0.0f) {
                            // Draw Progress Bar
                            float progress = timer / 4.0f;
                            int barW = 200;
                            
                            DrawRectangle(scrW/2 - barW/2, scrH/2 + 60, barW, 20, Fade(BLACK, 0.5f));
                            DrawRectangle(scrW/2 - barW/2, scrH/2 + 60, (int)(barW * progress), 20, LIME);
                            DrawRectangleLines(scrW/2 - barW/2, scrH/2 + 60, barW, 20, WHITE);
                            
                            // --- DYNAMIC TEXT LOGIC ---
                            const char* actionText = "PROCESSING..."; // Fallback
                            Vector2 pPos = { player.position.x, player.position.z };

                            for(int k=0; k<5; k++) {
                                DeliveryTask *t = &phone.tasks[k];
                                
                                // Case 1: At Restaurant (Job Accepted) -> Picking Up
                                if (t->status == JOB_ACCEPTED) {
                                    if (Vector2Distance(pPos, t->restaurantPos) < 20.0f) {
                                        actionText = "PICKING UP...";
                                        break;
                                    }
                                }
                                // Case 2: At House (Job Picked Up) -> Delivering
                                else if (t->status == JOB_PICKED_UP) {
                                    if (Vector2Distance(pPos, t->customerPos) < 20.0f) {
                                        actionText = "DELIVERING...";
                                        break;
                                    }
                                }
                            }
                            // ---------------------------

                            DrawText(actionText, scrW/2 - MeasureText(actionText, 20)/2, scrH/2 + 85, 20, WHITE);
                        } else {
                            // Draw Prompt
                            const char* txt = "HOLD [E] TO INTERACT";
                            DrawText(txt, scrW/2 - MeasureText(txt, 20)/2, scrH/2 + 60, 20, WHITE);
                        }
                    }
                    
                    DrawCargoHUD(&phone, &player);
                    
                    // [NEW] EMERGENCY RESCUE UI
                    if (player.fuel <= 0.0f) {
                        // Darken screen
                        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
                        
                        int cx = GetScreenWidth() / 2;
                        int cy = GetScreenHeight() / 2;

                        // Warning Icon / Text
                        const char* title = "OUT OF FUEL!";
                        DrawText(title, cx - MeasureText(title, 40)/2, cy - 100, 40, RED);

                        // Calculate Price for UI display
                        float emergencyPrice = 4.50f * 15.0f; // $67.50
                        
                        if (player.money < 25.0f) {
                            // Failsafe UI
                            DrawText("EMERGENCY ASSISTANCE", cx - MeasureText("EMERGENCY ASSISTANCE", 30)/2, cy - 40, 30, GREEN);
                            DrawText("Wallet empty. Government aid available.", cx - MeasureText("Wallet empty. Government aid available.", 20)/2, cy, 20, WHITE);
                            DrawText("Press [R] for FREE Emergency Fuel", cx - MeasureText("Press [R] for FREE Emergency Fuel", 20)/2, cy + 40, 20, YELLOW);
                        } else {
                            // Paid UI
                            DrawText("ROADSIDE ASSISTANCE", cx - MeasureText("ROADSIDE ASSISTANCE", 30)/2, cy - 40, 30, ORANGE);
                            
                            const char* costText = TextFormat("Cost: $%.2f (3x Normal Rate)", emergencyPrice);
                            DrawText(costText, cx - MeasureText(costText, 20)/2, cy, 20, WHITE);
                            
                            Color btnColor = (player.money >= emergencyPrice) ? YELLOW : GRAY;
                            const char* btnText = (player.money >= emergencyPrice) ? "Press [R] to Call Truck" : "Not Enough Money (Need < $25 for Aid)";
                            DrawText(btnText, cx - MeasureText(btnText, 20)/2, cy + 40, 20, btnColor);
                        }
                    }

                    // Interaction Text
                    if (!isDead && !isRefueling && !isMechanicOpen && fabs(player.current_speed) < 5.0f) {
                          Vector2 pPos2 = { player.position.x, player.position.z };
                          for(int i=0; i<map.locationCount; i++) {
                              Vector2 locPos2 = { map.locations[i].position.x + 2.0f, map.locations[i].position.y + 2.0f };
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
                if (isLoading) {
                // Returns false when bar hits 100%
                isLoading = DrawPostLoadOverlay(GetScreenWidth(), GetScreenHeight(), dt);
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
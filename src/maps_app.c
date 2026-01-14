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


#include "maps_app.h"
#include "raymath.h"
#include "map.h" 
#include "player.h"
#include <stdio.h>
#include <string.h>
#include <float.h> 
#include <ctype.h>

// --- MAPS APP STATE ---
typedef struct {
    Camera2D camera;
    bool isDragging;
    Vector2 dragStart;
    Vector2 playerPos; 
    float playerAngle;      
    bool isFollowingPlayer; 
    bool isHeadingUp;       
    float lastClickTime;
    Vector2 path[MAX_PATH_NODES];
    int pathLen;
    bool hasDestination;
    Vector2 destination;
    float lastPathUpdate;
    
    // Search State
    bool isSearching;
    char searchQuery[64];
    int searchCharCount;
    MapLocation searchResults[MAX_SEARCH_RESULTS];
    int resultCount;
    
    // Icons
    Texture2D icons[20]; 
    Texture2D pinIcon;
    Texture2D playerIcon;
    Texture2D emergencyIcon;
    
    // Filtering
    int filterType;       // -1 = All, else LOC_TYPE
    bool isFilterMenuOpen; 
} MapsAppState;

MapsAppState mapsState = {0};

float scale;

bool IsMapsAppTyping() {
    return mapsState.isSearching;
}

void LoadMapIcons() {
    mapsState.icons[LOC_FUEL]        = LoadTexture("resources/Mapicons/icon_fuel.png");
    mapsState.icons[LOC_FOOD]        = LoadTexture("resources/Mapicons/icon_fastfood.png");
    mapsState.icons[LOC_CAFE]        = LoadTexture("resources/Mapicons/icon_cafe.png");
    mapsState.icons[LOC_BAR]         = LoadTexture("resources/Mapicons/icon_bar.png");
    mapsState.icons[LOC_MARKET]      = LoadTexture("resources/Mapicons/icon_market.png");
    mapsState.icons[LOC_SUPERMARKET] = LoadTexture("resources/Mapicons/icon_supermarket.png");
    mapsState.icons[LOC_RESTAURANT]  = LoadTexture("resources/Mapicons/icon_restaurant.png");
    mapsState.icons[LOC_MECHANIC]    = LoadTexture("resources/Mapicons/icon_mechanic.png"); 
    mapsState.icons[LOC_DEALERSHIP]  = LoadTexture("resources/Mapicons/icon_dealership.png");
    mapsState.emergencyIcon = LoadTexture("resources/Mapicons/emergency.png");
    mapsState.pinIcon                = LoadTexture("resources/Mapicons/icon_pin.png");
    mapsState.playerIcon = LoadTexture("resources/Mapicons/icon_player.png");

    for(int i=0; i<20; i++) {
        if(mapsState.icons[i].id != 0) SetTextureFilter(mapsState.icons[i], TEXTURE_FILTER_BILINEAR);
    }
    if(mapsState.pinIcon.id != 0) SetTextureFilter(mapsState.pinIcon, TEXTURE_FILTER_BILINEAR);
}

// --- Math Helpers ---
Vector2 GetClosestPointOnSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ap = Vector2Subtract(p, a);
    Vector2 ab = Vector2Subtract(b, a);
    float abLenSq = Vector2LengthSqr(ab);
    if (abLenSq == 0.0f) return a; 
    float t = Vector2DotProduct(ap, ab) / abLenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return Vector2Add(a, Vector2Scale(ab, t));
}

Vector2 SnapToRoad(GameMap *map, Vector2 clickPos, float threshold) {
    Vector2 bestPoint = clickPos;
    float minDistSq = threshold * threshold;
    for (int i = 0; i < map->edgeCount; i++) {
        Vector2 start = map->nodes[map->edges[i].startNode].position;
        Vector2 end = map->nodes[map->edges[i].endNode].position;
        
        float minX = (start.x < end.x ? start.x : end.x) - threshold;
        float maxX = (start.x > end.x ? start.x : end.x) + threshold;
        float minY = (start.y < end.y ? start.y : end.y) - threshold;
        float maxY = (start.y > end.y ? start.y : end.y) + threshold;
        if (clickPos.x < minX || clickPos.x > maxX || clickPos.y < minY || clickPos.y > maxY) continue; 
        
        Vector2 closest = GetClosestPointOnSegment(clickPos, start, end);
        float dSq = Vector2DistanceSqr(clickPos, closest);
        if (dSq < minDistSq) { minDistSq = dSq; bestPoint = closest; }
    }
    return bestPoint;
}

void ToLowerStr(const char* src, char* dest) {
    for (int i = 0; src[i]; i++) {
        dest[i] = tolower((unsigned char)src[i]);
    }
    dest[strlen(src)] = '\0'; 
}

int SearchMapInternal(GameMap *map, const char *query, MapLocation *results) {
    int count = 0;
    char queryLower[64];
    char nameLower[64];

    ToLowerStr(query, queryLower);

    for (int i = 0; i < map->locationCount; i++) {
        if (map->locations[i].type == LOC_HOUSE) continue;

        ToLowerStr(map->locations[i].name, nameLower);

        if (strstr(nameLower, queryLower) != NULL) {
            results[count] = map->locations[i];
            count++;
            if (count >= MAX_SEARCH_RESULTS) break;
        }
    }
    return count;
}

void ShowRecommendedPlaces(GameMap *map) {
    mapsState.resultCount = 0;
    
    for (int i = 0; i < map->locationCount; i++) {
        if (map->locations[i].type == LOC_HOUSE) continue;
        
        if (mapsState.filterType != -1) {
            if (map->locations[i].type != mapsState.filterType) continue;
        }
        
        mapsState.searchResults[mapsState.resultCount] = map->locations[i];
        mapsState.resultCount++;
        if (mapsState.resultCount >= MAX_SEARCH_RESULTS) break;
    }
}

void InitMapsApp() {
    mapsState.camera.zoom = 4.0f; 
    mapsState.camera.offset = (Vector2){140, 280}; 
    mapsState.hasDestination = false;
    mapsState.isSearching = false;
    mapsState.searchCharCount = 0;
    mapsState.isFollowingPlayer = true; 
    mapsState.isHeadingUp = true; 
    mapsState.lastClickTime = 0.0f;
    mapsState.filterType = -1; 
    mapsState.isFilterMenuOpen = false;
    LoadMapIcons(); 
}

void ResetMapCamera(Vector2 playerPos) {
    mapsState.isFollowingPlayer = true;
    mapsState.isHeadingUp = true; 
    mapsState.camera.target = playerPos;
    mapsState.camera.zoom = 4.0f; 
    mapsState.isSearching = false;
    mapsState.isDragging = false;
    mapsState.isFilterMenuOpen = false;
}

void SetMapDestination(GameMap *map, Vector2 dest) {
    // 1. Try exact pathfinding first (fastest if it works)
    int len = FindPath(map, mapsState.playerPos, dest, mapsState.path, MAX_PATH_NODES);

    // 2. If exact path fails, snap to the closest road within a tight radius
    if (len == 0) {
        // Reduced radius from 500.0f to 20.0f for optimization. 
        // This is usually enough to span a sidewalk + parking strip.
        Vector2 closestRoadPoint = SnapToRoad(map, dest, 20.0f);
        
        // Try pathfinding again to this snapped point
        len = FindPath(map, mapsState.playerPos, closestRoadPoint, mapsState.path, MAX_PATH_NODES);
        
        // 3. Connect the street point to the building entrance visually
        if (len > 0 && len < MAX_PATH_NODES) {
            mapsState.path[len] = dest; 
            len++;
        }
    }

    // 4. Update State
    mapsState.destination = dest;
    mapsState.hasDestination = true;
    mapsState.isFollowingPlayer = true; 
    mapsState.isHeadingUp = true; 
    mapsState.pathLen = len;

    // Fallback: If still zero, the store is likely disconnected or bugged; draw direct line.
    if (len == 0) {
        mapsState.path[0] = mapsState.playerPos;
        mapsState.path[1] = dest;
        mapsState.pathLen = 2;
    }
}

void PreviewMapLocation(GameMap *map, Vector2 target) {
    int len = FindPath(map, mapsState.playerPos, target, mapsState.path, MAX_PATH_NODES);
    
    mapsState.destination = target;
    mapsState.hasDestination = true;
    mapsState.pathLen = len;

    mapsState.isFollowingPlayer = false; 
    
    mapsState.camera.target = target;
    mapsState.camera.zoom = 3.0f; 
}

void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, float playerAngle, Vector2 localMouse, bool isClicking) {
    mapsState.playerPos = currentPlayerPos; 
    mapsState.playerAngle = playerAngle;

    if (mapsState.hasDestination) {
        // 1. Try standard pathfinding
        mapsState.pathLen = FindPath(map, mapsState.playerPos, mapsState.destination, mapsState.path, MAX_PATH_NODES);

        // [FIX] If standard pathfinding fails (e.g., store is in a parking lot/off-road)
        if (mapsState.pathLen == 0) {
            
            // OPTIONAL: Try to snap to the closest road dynamically (if map isn't huge)
            // Using a small radius (60.0) ensures it's fast enough for the Update loop.
            Vector2 snappedPos = SnapToRoad(map, mapsState.destination, 60.0f);
            
            // Try pathfinding to the road edge
            int roadLen = FindPath(map, mapsState.playerPos, snappedPos, mapsState.path, MAX_PATH_NODES);
            
            if (roadLen > 0 && roadLen < MAX_PATH_NODES) {
                // Success! We found a path to the street.
                // Now manually draw the final line from street -> store
                mapsState.path[roadLen] = mapsState.destination;
                mapsState.pathLen = roadLen + 1;
            } else {
                // Total failure (no roads nearby) -> Fallback to direct Straight Line
                mapsState.path[0] = mapsState.playerPos;
                mapsState.path[1] = mapsState.destination;
                mapsState.pathLen = 2;
            }
        }

        // Arrival check
        if (Vector2Distance(mapsState.playerPos, mapsState.destination) < 0.5f) { // Increased radius slightly to 15.0f for smoother arrival
            mapsState.hasDestination = false;
            mapsState.pathLen = 0;
        }
    }

    if (mapsState.isFollowingPlayer && !mapsState.isDragging && !mapsState.isSearching) {
        Vector2 diff = Vector2Subtract(mapsState.playerPos, mapsState.camera.target);
        if (Vector2Length(diff) > 0.1f) {
            mapsState.camera.target = Vector2Add(mapsState.camera.target, Vector2Scale(diff, 0.3f));
        }

        if (mapsState.isHeadingUp) {
            float targetRot = playerAngle + 180.0f; 
            float currentRot = mapsState.camera.rotation;
            float rotDiff = targetRot - currentRot;
            while (rotDiff < -180) rotDiff += 360;
            while (rotDiff > 180) rotDiff -= 360;
            mapsState.camera.rotation += rotDiff * 0.2f;
        } else {
            mapsState.camera.rotation = Lerp(mapsState.camera.rotation, 0.0f, 0.2f);
        }
    }

    if (localMouse.x < 0 || localMouse.x > 280 || localMouse.y < 0 || localMouse.y > 600) {
        mapsState.isDragging = false;
        return; 
    }

    if (isClicking) {
        bool handled = false;
        
        if (mapsState.isFilterMenuOpen) {
            Rectangle menuRect = {190, 75, 80, 100}; 
            if (CheckCollisionPointRec(localMouse, menuRect)) {
                float relY = localMouse.y - 75;
                int idx = (int)(relY / 25);
                
                if (idx == 0) mapsState.filterType = -1;           
                else if (idx == 1) mapsState.filterType = LOC_FUEL;     
                else if (idx == 2) mapsState.filterType = LOC_MECHANIC; 
                else if (idx == 3) mapsState.filterType = LOC_FOOD;     
                
                mapsState.isFilterMenuOpen = false;
                if (mapsState.isSearching && mapsState.searchCharCount == 0) {
                    ShowRecommendedPlaces(map);
                }
                handled = true;
            } else {
                mapsState.isFilterMenuOpen = false;
                handled = true; 
            }
        }

        if (!handled) {
            Rectangle filterBtn = {190, 40, 30, 30};
            if (CheckCollisionPointRec(localMouse, filterBtn)) {
                mapsState.isFilterMenuOpen = !mapsState.isFilterMenuOpen;
                handled = true;
            }
        }

        if (!handled && mapsState.isSearching) {
            for(int i=0; i<mapsState.resultCount; i++) {
                Rectangle resRect = {10, 80 + i*45, 260, 40};
                if (CheckCollisionPointRec(localMouse, resRect)) {
                    SetMapDestination(map, mapsState.searchResults[i].position);
                    mapsState.isSearching = false;
                    handled = true;
                    break;
                }
            }
        }

        if (!handled) {
            if (localMouse.y >= 40 && localMouse.y <= 70) {
                 if (localMouse.x >= 10 && localMouse.x <= 180) {
                      mapsState.isSearching = true;
                      if (mapsState.searchCharCount == 0) ShowRecommendedPlaces(map);
                 }
                 else if (localMouse.x >= 230 && localMouse.x <= 260) {
                      ResetMapCamera(mapsState.playerPos);
                 }
                 handled = true; 
            }
            else {
                if (CheckCollisionPointCircle(localMouse, (Vector2){240, 450}, 20)) {
                    mapsState.isHeadingUp = false; 
                    handled = true;
                }
                else if (!mapsState.isFollowingPlayer && 
                         CheckCollisionPointCircle(localMouse, (Vector2){240, 510}, 25)) {
                    mapsState.isFollowingPlayer = true;
                    mapsState.isHeadingUp = true; 
                    handled = true;
                }
            }

            if (!handled) {
                float currentTime = GetTime();
                if ((currentTime - mapsState.lastClickTime) < 0.3f) { 
                    Vector2 worldPos = GetScreenToWorld2D(localMouse, mapsState.camera);
                    Vector2 snappedPos = SnapToRoad(map, worldPos, 30.0f);
                    SetMapDestination(map, snappedPos);
                }
                mapsState.lastClickTime = currentTime;
                mapsState.isDragging = true;
                mapsState.dragStart = GetScreenToWorld2D(localMouse, mapsState.camera);
                mapsState.isSearching = false;    
                mapsState.isFollowingPlayer = false; 
                mapsState.isFilterMenuOpen = false; 
            }
        }
    }
    
    if (mapsState.isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(localMouse, mapsState.camera);
            Vector2 delta = Vector2Subtract(mapsState.dragStart, mouseWorldPos);
            mapsState.camera.target = Vector2Add(mapsState.camera.target, delta);
        } else {
            mapsState.isDragging = false;
        }
    }
    
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mouseWorldBeforeZoom = GetScreenToWorld2D(localMouse, mapsState.camera);
        mapsState.camera.zoom += wheel * 0.5f; 
        if (mapsState.camera.zoom < 0.5f) mapsState.camera.zoom = 0.5f;
        if (mapsState.camera.zoom > 10.0f) mapsState.camera.zoom = 10.0f;
        Vector2 mouseWorldAfterZoom = GetScreenToWorld2D(localMouse, mapsState.camera);
        Vector2 zoomDelta = Vector2Subtract(mouseWorldBeforeZoom, mouseWorldAfterZoom);
        mapsState.camera.target = Vector2Add(mapsState.camera.target, zoomDelta);
    }

    if (mapsState.isSearching) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (mapsState.searchCharCount < 63)) {
                mapsState.searchQuery[mapsState.searchCharCount] = (char)key;
                mapsState.searchQuery[mapsState.searchCharCount + 1] = '\0';
                mapsState.searchCharCount++;
                mapsState.resultCount = SearchMapInternal(map, mapsState.searchQuery, mapsState.searchResults);
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (mapsState.searchCharCount > 0) {
                mapsState.searchCharCount--;
                mapsState.searchQuery[mapsState.searchCharCount] = '\0';
                
                if (mapsState.searchCharCount == 0) ShowRecommendedPlaces(map);
                else mapsState.resultCount = SearchLocations(map, mapsState.searchQuery, mapsState.searchResults);
            }
        }
        if (IsKeyPressed(KEY_ENTER) && mapsState.resultCount > 0) {
            SetMapDestination(map, mapsState.searchResults[0].position);
            mapsState.isSearching = false;
        }
    }
}

void DrawMapsApp(GameMap *map) {
    ClearBackground(RAYWHITE);
    BeginMode2D(mapsState.camera);
    scale = 1.0f / mapsState.camera.zoom;

    // --- MAP RENDER (Optimized) ---
    DrawMap2DView(map, mapsState.camera, 280.0f, 600.0f);

    // Events
    for(int i=0; i<MAX_EVENTS; i++) {
        if(map->events[i].active) {
            
            // IF THE ICON LOADED SUCCESSFULLY:
            if (mapsState.emergencyIcon.id != 0) {
                float iconWorldSize = 12.0f; // Adjust size as needed
                
                Rectangle source = { 0.0f, 0.0f, (float)mapsState.emergencyIcon.width, (float)mapsState.emergencyIcon.height };
                Rectangle dest = { map->events[i].position.x, map->events[i].position.y, iconWorldSize, iconWorldSize };
                Vector2 origin = { iconWorldSize/2, iconWorldSize/2 }; // Center the icon
                
                // Draw texture counter-rotated so it stays upright
                DrawTexturePro(mapsState.emergencyIcon, source, dest, origin, -mapsState.camera.rotation, WHITE);
            } 
            // FALLBACK (If image missing, draw the old red circle)
            else {
                Color c = RED;
                DrawCircleV(map->events[i].position, 8.0f * scale, c);
                DrawText("!", 
                         map->events[i].position.x - 2*scale, 
                         map->events[i].position.y - 4*scale, 
                         10.0f * scale, WHITE);
            }
        }
    }

    // --- POIs (Icons) ---
    float screenW = 280.0f;
    float screenH = 600.0f;
    float border = 30.0f; 

    for(int i=0; i<map->locationCount; i++) {
        if (map->locations[i].type == LOC_HOUSE) continue; 
        
        // Filter Check
        if (mapsState.filterType != -1 && map->locations[i].type != mapsState.filterType) continue;

        // Culling check
        Vector2 screenPos = GetWorldToScreen2D(map->locations[i].position, mapsState.camera);
        if (screenPos.x < -border || screenPos.x > screenW + border || 
            screenPos.y < -border || screenPos.y > screenH + border) {
            continue;
        }

        int type = map->locations[i].type;
        Texture2D icon = mapsState.icons[type];
        Vector2 pos = map->locations[i].position;
        
        float worldSize = 7.15f; 
        
        if (icon.id != 0) {
            Rectangle source = { 0.0f, 0.0f, (float)icon.width, (float)icon.height };
            Rectangle dest = { pos.x, pos.y, worldSize, worldSize };
            Vector2 origin = { worldSize/2, worldSize/2 };
            
            // Counter-rotate icons so they stay Upright
            DrawTexturePro(icon, source, dest, origin, -mapsState.camera.rotation, WHITE);
        } 
        else {
            Color c = GRAY;
            switch(map->locations[i].type) {
                case LOC_FUEL: c = ORANGE; break;
                case LOC_MECHANIC: c = BLUE; break;
                case LOC_FOOD: c = RED; break;
                case LOC_MARKET: c = BLACK; break;
                case LOC_RESTAURANT: c = MAROON; break;
                case LOC_CAFE: c = BROWN; break;
                default: c = DARKGRAY; break;
            }
            DrawCircleV(pos, 3.6f * scale, c); 
        }
        
        // Text Labels
        if (mapsState.camera.zoom > 5.0f) { 
            float targetScreenSize = 20.0f; 
            float fontSize = targetScreenSize / mapsState.camera.zoom;
            Vector2 textSize = MeasureTextEx(GetFontDefault(), map->locations[i].name, fontSize, 1.0f);
            
            Vector2 origin = { textSize.x / 2, -(10.0f / mapsState.camera.zoom) };
            DrawTextPro(GetFontDefault(), map->locations[i].name, pos, origin, -mapsState.camera.rotation, fontSize, 1.0f, BLACK);
        }
    }

    // --- PLAYER ICON ---
    if (mapsState.playerIcon.id != 0) {
        float pSize = 52.0f * scale; 
        Rectangle src = {0, 0, (float)mapsState.playerIcon.width, (float)mapsState.playerIcon.height};
        Rectangle dst = {mapsState.playerPos.x, mapsState.playerPos.y, pSize, pSize};
        Vector2 origin = {pSize/2, pSize/2};
        
        // [FIX] Player Rotation Logic
        // 1. Negate playerAngle to match Raylib 2D rotation direction (CCW vs CW)
        // 2. Subtract 90 degrees for base CCW offset.
        float finalPlayerAngle = -mapsState.playerAngle - 180.0f;

        DrawTexturePro(mapsState.playerIcon, src, dst, origin, finalPlayerAngle, WHITE);
    } else {
        // Fallback
        DrawCircleV(mapsState.playerPos, 10.0f * scale, GREEN);
        Vector2 tip = {
            mapsState.playerPos.x + sinf(mapsState.playerAngle*DEG2RAD)*8.0f*scale,
            mapsState.playerPos.y + cosf(mapsState.playerAngle*DEG2RAD)*8.0f*scale
        };
        DrawLineEx(mapsState.playerPos, tip, 2.0f * scale, DARKBLUE);
    }

    // --- PATH & PIN ---
    if (mapsState.hasDestination && mapsState.pathLen > 0.2) {
        float pathThick = 8.0f * scale; 
        for (int i = 0; i < mapsState.pathLen - 1; i++) {
            DrawLineEx(mapsState.path[i], mapsState.path[i+1], pathThick, RED);
        }
        DrawLineEx(mapsState.path[mapsState.pathLen-1], mapsState.destination, pathThick, RED);
        
        // Draw Pin Icon at destination (Rotated upright)
        if (mapsState.pinIcon.id != 0) {
            float pinSize = 24.0f * scale; 
            Rectangle src = {0, 0, (float)mapsState.pinIcon.width, (float)mapsState.pinIcon.height};
            Rectangle dst = {mapsState.destination.x, mapsState.destination.y, pinSize, pinSize};
            Vector2 origin = {pinSize/2, pinSize}; // Anchor at bottom center
            DrawTexturePro(mapsState.pinIcon, src, dst, origin, -mapsState.camera.rotation, WHITE);
        } else {
            DrawCircleV(mapsState.destination, 5.0f * scale, RED);
        }
    }

    EndMode2D();

    // --- UI OVERLAY (Standard) ---
    // ... (The rest of the function remains exactly the same) ...
    DrawRectangle(0, 0, 280, 80, WHITE);
    DrawText("Maps", 10, 10, 20, BLACK);
    
    // Search Bar
    DrawRectangle(10, 40, 180, 30, LIGHTGRAY);
    if (mapsState.isSearching) {
        DrawText(mapsState.searchQuery, 15, 48, 10, BLACK);
        if ((GetTime() * 2.0f) - (int)(GetTime() * 2.0f) < 0.5f) {
            DrawRectangle(15 + MeasureText(mapsState.searchQuery, 10), 48, 2, 16, BLACK);
        }
    } else {
        DrawText("Search...", 15, 48, 10, GRAY);
    }
    
    // Filter Button
    Color filterCol = (mapsState.filterType != -1) ? BLUE : LIGHTGRAY;
    DrawRectangle(200, 40, 30, 30, filterCol);
    DrawRectangleLines(200, 40, 30, 30, DARKGRAY);
    DrawLine(205, 48, 225, 48, BLACK);
    DrawLine(208, 54, 222, 54, BLACK);
    DrawLine(212, 60, 218, 60, BLACK);

    // Close Button
    DrawRectangle(240, 40, 30, 30, BLACK);
    DrawText("X", 250, 48, 10, WHITE);

    // Filter Dropdown
    if (mapsState.isFilterMenuOpen) {
        Rectangle menuRect = {190, 75, 80, 100};
        DrawRectangleRec(menuRect, WHITE);
        DrawRectangleLinesEx(menuRect, 1, DARKGRAY);
        
        const char* opts[] = {"All", "Gas", "Mech", "Food"};
        for(int i=0; i<4; i++) {
            bool selected = false;
            if(i==0 && mapsState.filterType == -1) selected = true;
            if(i==1 && mapsState.filterType == LOC_FUEL) selected = true;
            if(i==2 && mapsState.filterType == LOC_MECHANIC) selected = true;
            if(i==3 && mapsState.filterType == LOC_FOOD) selected = true;
            
            Color itemColor = selected ? SKYBLUE : WHITE;
            if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){190, 75 + i*25, 80, 25})) itemColor = LIGHTGRAY;
            
            DrawRectangle(190, 75 + i*25, 80, 25, itemColor);
            DrawText(opts[i], 195, 80 + i*25, 10, BLACK);
            DrawLine(190, 100 + i*25, 270, 100 + i*25, LIGHTGRAY);
        }
    }

    // Search Results
    if (mapsState.isSearching && mapsState.resultCount > 0) {
        DrawRectangle(10, 80, 260, mapsState.resultCount * 45, WHITE);
        DrawRectangleLines(10, 80, 260, mapsState.resultCount * 45, LIGHTGRAY);
        
        for(int i=0; i<mapsState.resultCount; i++) {
            float yPos = 85 + i*45;
            Rectangle itemRect = {10, 80 + i*45, 260, 45};
            if (CheckCollisionPointRec(GetMousePosition(), itemRect)) { 
                DrawRectangleRec(itemRect, Fade(SKYBLUE, 0.3f));
            }
            DrawText(mapsState.searchResults[i].name, 20, yPos, 20, BLACK);
            const char* typeStr = "Location";
            if (mapsState.searchResults[i].type == LOC_FUEL) typeStr = "Gas Station";
            if (mapsState.searchResults[i].type == LOC_MECHANIC) typeStr = "Mechanic";
            DrawText(typeStr, 20, yPos + 20, 10, GRAY);
            DrawLine(10, yPos + 40, 250, yPos + 40, LIGHTGRAY);
        }
    }

    // Distance Label
    if (mapsState.hasDestination) {
        float dist = 0.0f;

        // [NEW] Calculate Total Road Distance by summing path segments
        if (mapsState.pathLen > 0) {
            // 1. Distance from Player to the start of the path
            dist += Vector2Distance(mapsState.playerPos, mapsState.path[0]);

            // 2. Sum of all internal road segments
            for (int i = 0; i < mapsState.pathLen - 1; i++) {
                dist += Vector2Distance(mapsState.path[i], mapsState.path[i+1]);
            }

            // 3. Distance from the last path node to the actual destination pin
            dist += Vector2Distance(mapsState.path[mapsState.pathLen - 1], mapsState.destination);
        } 
        else {
            // Fallback: If no path nodes (very close), use straight line
            dist = Vector2Distance(mapsState.playerPos, mapsState.destination);
        }

        // Apply your map scale factor (from your original code)
        dist *= 5.0;

        char distText[32];
        if (dist >= 1000.0f) {
            snprintf(distText, 32, "%.1f km to Target", dist / 1000.0f);
        } else {
            snprintf(distText, 32, "%d m to Target", (int)dist);
        }
        DrawText(distText, 20, 508, 20, BLACK); 
    }

    // Compass
    DrawCircle(240, 450, 20, WHITE);
    DrawCircleLines(240, 450, 20, DARKGRAY);
    float needleRot = -mapsState.camera.rotation * DEG2RAD;
    Vector2 cCenter = {240, 450};
    Vector2 northTip = { cCenter.x + sinf(needleRot)*15, cCenter.y - cosf(needleRot)*15 };
    Vector2 southTip = { cCenter.x - sinf(needleRot)*15, cCenter.y + cosf(needleRot)*15 };
    DrawLineEx(cCenter, northTip, 3.0f, RED);   
    DrawLineEx(cCenter, southTip, 3.0f, DARKGRAY); 

    // Re-center
    if (!mapsState.isFollowingPlayer) {
        DrawCircle(240, 510, 25, BLUE);
        DrawCircleLines(240, 510, 25, WHITE);
        DrawText("O", 233, 502, 20, WHITE); 
    }
}

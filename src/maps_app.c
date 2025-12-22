#include "maps_app.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>
#include <float.h> 

// --- MAPS APP STATE ---
typedef struct {
    Camera2D camera;
    
    // Interaction
    bool isDragging;
    Vector2 dragStart;
    Vector2 playerPos; 
    float playerAngle;      
    
    bool isFollowingPlayer; 
    bool isHeadingUp;       

    // Input Timing
    float lastClickTime;
    Vector2 lastClickPos;

    // Pathfinding
    Vector2 path[MAX_PATH_NODES];
    int pathLen;
    bool hasDestination;
    Vector2 destination;

    // Search
    bool isSearching;
    char searchQuery[64];
    int searchCharCount;
    MapLocation searchResults[MAX_SEARCH_RESULTS];
    int resultCount;
} MapsAppState;

MapsAppState mapsState = {0};

bool IsMapsAppTyping() {
    return mapsState.isSearching;
}

// --- MATH HELPERS ---
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
        if (dSq < minDistSq) {
            minDistSq = dSq;
            bestPoint = closest;
        }
    }
    return bestPoint;
}

void ShowRecommendedPlaces(GameMap *map) {
    mapsState.resultCount = SearchLocations(map, "a", mapsState.searchResults);
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
}

void ResetMapCamera(Vector2 playerPos) {
    mapsState.isFollowingPlayer = true;
    mapsState.isHeadingUp = true; 
    mapsState.camera.target = playerPos;
    mapsState.camera.zoom = 4.0f; 
    mapsState.isSearching = false;
    mapsState.isDragging = false;
}

void SetMapDestination(GameMap *map, Vector2 dest) {
    int len = FindPath(map, mapsState.playerPos, dest, mapsState.path, MAX_PATH_NODES);
    
    if (len == 0) {
        float dist = Vector2Distance(mapsState.playerPos, dest);
        if (dist < 10.0f) {
            mapsState.destination = dest;
            mapsState.hasDestination = false; 
            return;
        }
        return; 
    }

    mapsState.destination = dest;
    mapsState.hasDestination = true;
    mapsState.pathLen = len;
    mapsState.isFollowingPlayer = true; 
    mapsState.isHeadingUp = true; 
}

void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, float playerAngle, Vector2 localMouse, bool isClicking) {
    mapsState.playerPos = currentPlayerPos; 
    mapsState.playerAngle = playerAngle;

    // --- 1. END NAVIGATION ---
    if (mapsState.hasDestination) {
        if (Vector2Distance(mapsState.playerPos, mapsState.destination) < 5.0f) {
            mapsState.hasDestination = false;
            mapsState.pathLen = 0;
        }
    }

    // --- 2. FOLLOW & ROTATION LOGIC ---
    if (mapsState.isFollowingPlayer && !mapsState.isDragging) {
        Vector2 diff = Vector2Subtract(mapsState.playerPos, mapsState.camera.target);
        if (Vector2Length(diff) > 0.1f) {
            mapsState.camera.target = Vector2Add(mapsState.camera.target, Vector2Scale(diff, 0.1f));
        }

        if (mapsState.isHeadingUp) {
            float targetRot = playerAngle; 
            float currentRot = mapsState.camera.rotation;
            float rotDiff = targetRot - currentRot;
            while (rotDiff < -180) rotDiff += 360;
            while (rotDiff > 180) rotDiff -= 360;
            mapsState.camera.rotation += rotDiff * 0.1f;
        } else {
            mapsState.camera.rotation = Lerp(mapsState.camera.rotation, 0.0f, 0.1f);
        }
    }

    // --- 3. MOUSE INTERACTIONS ---
    if (isClicking) {
        bool handled = false;

        // A. SEARCH RESULTS
        if (mapsState.isSearching) {
            for(int i=0; i<mapsState.resultCount; i++) {
                Rectangle resRect = {10, 85 + i*45, 260, 40};
                if (CheckCollisionPointRec(localMouse, resRect)) {
                    SetMapDestination(map, mapsState.searchResults[i].position);
                    mapsState.isSearching = false;
                    handled = true;
                    break;
                }
            }
        }

        if (!handled) {
            // B. UI HEADER
            if (localMouse.y <= 80) {
                 if (localMouse.x < 200 && localMouse.y > 30) {
                     mapsState.isSearching = true;
                     if (mapsState.searchCharCount == 0) ShowRecommendedPlaces(map);
                 }
                 else if (localMouse.x >= 200 && localMouse.y > 30) {
                     ResetMapCamera(mapsState.playerPos);
                 }
                 handled = true; 
            }
            // C. BUTTONS
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

            // D. MAP CLICK
            if (!handled) {
                float currentTime = GetTime();
                if ((currentTime - mapsState.lastClickTime) < 0.5f) {
                    Vector2 worldPos = GetScreenToWorld2D(localMouse, mapsState.camera);
                    Vector2 snappedPos = SnapToRoad(map, worldPos, 30.0f);
                    SetMapDestination(map, snappedPos);
                }
                mapsState.lastClickTime = currentTime;

                mapsState.isDragging = true;
                mapsState.dragStart = localMouse; 
                mapsState.isSearching = false;    
                mapsState.isFollowingPlayer = false; 
            }
        }
    }
    
    // --- 4. DRAGGING ---
    if (mapsState.isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(localMouse, mapsState.camera);
            Vector2 startWorldPos = GetScreenToWorld2D(mapsState.dragStart, mapsState.camera);
            Vector2 delta = Vector2Subtract(startWorldPos, mouseWorldPos);
            mapsState.camera.target = Vector2Add(mapsState.camera.target, delta);
            mapsState.dragStart = localMouse;
        } else {
            mapsState.isDragging = false;
        }
    }
    
    // --- 5. ZOOM ---
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        mapsState.camera.zoom += wheel * 0.5f; 
        if (mapsState.camera.zoom < 0.5f) mapsState.camera.zoom = 0.5f;
        if (mapsState.camera.zoom > 10.0f) mapsState.camera.zoom = 10.0f;
    }

    // --- 6. KEYBOARD ---
    if (mapsState.isSearching) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (mapsState.searchCharCount < 63)) {
                mapsState.searchQuery[mapsState.searchCharCount] = (char)key;
                mapsState.searchQuery[mapsState.searchCharCount + 1] = '\0';
                mapsState.searchCharCount++;
                mapsState.resultCount = SearchLocations(map, mapsState.searchQuery, mapsState.searchResults);
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
    
    float scale = 1.0f / mapsState.camera.zoom;

    // --- 1. DRAW AREAS (Parks & Water) ---
    for (int i = 0; i < map->areaCount; i++) {
        // Simple fan triangulation to fill polygons
        // Note: Assumes convex shapes roughly. 
        if(map->areas[i].pointCount < 3) continue;
        
        Vector2 center = {0,0};
        for(int j=0; j<map->areas[i].pointCount; j++) center = Vector2Add(center, map->areas[i].points[j]);
        center = Vector2Scale(center, 1.0f/map->areas[i].pointCount);

        for (int j = 0; j < map->areas[i].pointCount; j++) {
            Vector2 p1 = map->areas[i].points[j];
            Vector2 p2 = map->areas[i].points[(j+1) % map->areas[i].pointCount];
            DrawTriangle(center, p2, p1, map->areas[i].color);
        }
    }

    // --- 2. DRAW ROADS ---
    float lineThick = 5.0f * scale; 
    if (lineThick < 1.0f) lineThick = 1.0f;

    for (int i = 0; i < map->edgeCount; i++) {
        Vector2 s = map->nodes[map->edges[i].startNode].position;
        Vector2 e = map->nodes[map->edges[i].endNode].position;
        DrawLineEx(s, e, map->edges[i].width, LIGHTGRAY);
        // Optional: Draw centerline for visual detail
        if (map->edges[i].width > 5.0f && mapsState.camera.zoom > 2.0f) {
            DrawLineEx(s, e, 1.0f * scale, WHITE);
        }
    }
    
    // --- 3. DRAW BUILDINGS ---
    for (int i = 0; i < map->buildingCount; i++) {
        // Just draw footprint outline for 2D map
        for(int j=0; j<map->buildings[i].pointCount; j++) {
            Vector2 p1 = map->buildings[i].footprint[j];
            Vector2 p2 = map->buildings[i].footprint[(j+1)%map->buildings[i].pointCount];
            DrawLineEx(p1, p2, 2.0f * scale, DARKGRAY);
        }
        // Fill semi-transparent
        DrawTriangleFan(map->buildings[i].footprint, map->buildings[i].pointCount, Fade(map->buildings[i].color, 0.5f));
    }

    // --- 4. DRAW TRAFFIC SIGNALS ---
    if (mapsState.camera.zoom > 3.0f) {
        for (int i = 0; i < map->nodeCount; i++) {
            if (map->nodes[i].flags == 1) { // Traffic Light
                DrawCircleV(map->nodes[i].position, 3.0f * scale, YELLOW);
            } else if (map->nodes[i].flags == 2) { // Stop Sign
                DrawCircleV(map->nodes[i].position, 3.0f * scale, RED);
            }
        }
    }

    // --- 5. DRAW POIs ---
    for(int i=0; i<map->locationCount; i++) {
        Color c = GRAY;
        switch(map->locations[i].type) {
            case LOC_FUEL: c = ORANGE; break;
            case LOC_FOOD: c = RED; break;
            case LOC_MARKET: c = BLUE; break;
            case LOC_RESTAURANT: c = MAROON; break;
            case LOC_CAFE: c = BROWN; break;
            default: c = DARKGRAY; break;
        }
        if (map->locations[i].type != LOC_HOUSE) {
            DrawCircleV(map->locations[i].position, 6.0f * scale, c);
            if (mapsState.camera.zoom > 5.0f) {
                // Draw tiny text label if zoomed in
                DrawText(map->locations[i].name, 
                         map->locations[i].position.x, 
                         map->locations[i].position.y, 
                         100 * scale, BLACK);
            }
        }
    }

    // Draw Player
    DrawCircleV(mapsState.playerPos, 4.0f * scale, BLUE);
    Vector2 tip = {
        mapsState.playerPos.x + sinf(mapsState.playerAngle*DEG2RAD)*8.0f*scale,
        mapsState.playerPos.y + cosf(mapsState.playerAngle*DEG2RAD)*8.0f*scale
    };
    DrawLineEx(mapsState.playerPos, tip, 2.0f * scale, DARKBLUE);

    // Draw Path
    if (mapsState.hasDestination && mapsState.pathLen > 1) {
        float pathThick = 8.0f * scale; 
        for (int i = 0; i < mapsState.pathLen - 1; i++) {
            DrawLineEx(mapsState.path[i], mapsState.path[i+1], pathThick, RED);
        }
        DrawLineEx(mapsState.path[mapsState.pathLen-1], mapsState.destination, pathThick, RED);
        DrawCircleV(mapsState.destination, 5.0f * scale, RED);
    }

    EndMode2D();

    // --- UI OVERLAY ---
    
    DrawRectangle(0, 0, 280, 80, WHITE);
    DrawText("Maps", 10, 10, 20, BLACK);

    // Search Box
    DrawRectangle(10, 40, 200, 30, LIGHTGRAY);
    if (mapsState.isSearching) {
        DrawText(mapsState.searchQuery, 15, 48, 10, BLACK);
        if ((GetTime() * 2.0f) - (int)(GetTime() * 2.0f) < 0.5f) {
            DrawRectangle(15 + MeasureText(mapsState.searchQuery, 10), 48, 2, 16, BLACK);
        }
    } else {
        DrawText("Search...", 15, 48, 10, GRAY);
    }

    // Reset Search Button
    DrawRectangle(220, 40, 50, 30, BLACK);
    DrawText("X", 240, 50, 10, WHITE);

    // Results Dropdown
    if (mapsState.isSearching && mapsState.resultCount > 0) {
        DrawRectangle(10, 80, 260, mapsState.resultCount * 45, WHITE);
        for(int i=0; i<mapsState.resultCount; i++) {
            float yPos = 85 + i*45;
            Rectangle itemRect = {10, 80 + i*45, 260, 45};
            if (CheckCollisionPointRec(GetMousePosition(), itemRect)) { 
                DrawRectangleRec(itemRect, Fade(SKYBLUE, 0.3f));
            }
            DrawText(mapsState.searchResults[i].name, 20, yPos, 20, BLACK);
            DrawText("Location", 20, yPos + 20, 10, GRAY);
            DrawLine(10, yPos + 40, 250, yPos + 40, LIGHTGRAY);
        }
    }

    // --- BUTTONS ---
    
    // 1. COMPASS BUTTON
    DrawCircle(240, 450, 20, WHITE);
    DrawCircleLines(240, 450, 20, DARKGRAY);
    
    float needleRot = -mapsState.camera.rotation * DEG2RAD;
    Vector2 center = {240, 450};
    Vector2 northTip = { center.x + sinf(needleRot)*15, center.y - cosf(needleRot)*15 };
    Vector2 southTip = { center.x - sinf(needleRot)*15, center.y + cosf(needleRot)*15 };
    DrawLineEx(center, northTip, 3.0f, RED);   
    DrawLineEx(center, southTip, 3.0f, DARKGRAY); 

    // 2. RE-CENTER BUTTON
    if (!mapsState.isFollowingPlayer) {
        DrawCircle(240, 510, 25, BLACK);
        DrawCircleLines(240, 510, 25, WHITE);
        DrawText("O", 233, 502, 20, WHITE); 
    }
}
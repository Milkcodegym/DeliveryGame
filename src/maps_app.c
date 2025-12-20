#include "maps_app.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>
#include <float.h> // For FLT_MAX

// --- MAPS APP STATE ---
typedef struct {
    Camera2D camera;
    
    // Interaction State
    bool isDragging;
    Vector2 dragStart;
    Vector2 playerPos; 
    
    bool isFollowingPlayer; 

    // Double Click State
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

// --- HELPER: MATH FOR SNAPPING ---
Vector2 GetClosestPointOnSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ap = Vector2Subtract(p, a);
    Vector2 ab = Vector2Subtract(b, a);
    
    float abLenSq = Vector2LengthSqr(ab);
    if (abLenSq == 0.0f) return a; // a and b are same
    
    // Project point onto line, clamp between 0 and 1
    float t = Vector2DotProduct(ap, ab) / abLenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    return Vector2Add(a, Vector2Scale(ab, t));
}

// Scans all roads to find the closest point on a road center-line
Vector2 SnapToRoad(GameMap *map, Vector2 clickPos, float threshold) {
    Vector2 bestPoint = clickPos;
    float minDistSq = threshold * threshold;
    bool found = false;

    for (int i = 0; i < map->edgeCount; i++) {
        Vector2 start = map->nodes[map->edges[i].startNode].position;
        Vector2 end = map->nodes[map->edges[i].endNode].position;

        // --- OPTIMIZATION: AABB (Bounding Box) Check ---
        // We calculate the rectangle (bounding box) of the road segment
        // plus the threshold buffer. If the click isn't in this box, skip the heavy math.
        
        float minX = (start.x < end.x ? start.x : end.x) - threshold;
        float maxX = (start.x > end.x ? start.x : end.x) + threshold;
        float minY = (start.y < end.y ? start.y : end.y) - threshold;
        float maxY = (start.y > end.y ? start.y : end.y) + threshold;

        if (clickPos.x < minX || clickPos.x > maxX || 
            clickPos.y < minY || clickPos.y > maxY) {
            continue; // Skip this road, it's too far away
        }

        // --- HEAVY MATH ---
        // Only runs for roads that are actually close to the click
        Vector2 closest = GetClosestPointOnSegment(clickPos, start, end);
        float dSq = Vector2DistanceSqr(clickPos, closest);
        
        if (dSq < minDistSq) {
            minDistSq = dSq;
            bestPoint = closest;
            found = true;
        }
    }
    
    return bestPoint;
}

void ShowRecommendedPlaces() {
    mapsState.resultCount = SearchLocations("a", mapsState.searchResults);
}

void InitMapsApp() {
    mapsState.camera.zoom = 4.0f; 
    mapsState.camera.offset = (Vector2){200, 300}; 
    mapsState.hasDestination = false;
    mapsState.isSearching = false;
    mapsState.searchCharCount = 0;
    mapsState.isFollowingPlayer = true; 
    mapsState.lastClickTime = 0.0f;
}

void ResetMapCamera(Vector2 playerPos) {
    mapsState.isFollowingPlayer = true;
    mapsState.camera.target = playerPos;
    mapsState.camera.zoom = 4.0f; 
    mapsState.isSearching = false;
    mapsState.isDragging = false;
}

void SetMapDestination(GameMap *map, Vector2 dest) {
    mapsState.destination = dest;
    mapsState.hasDestination = true;
    mapsState.pathLen = FindPath(map, mapsState.playerPos, mapsState.destination, mapsState.path, MAX_PATH_NODES);
    mapsState.isFollowingPlayer = true; 
}

void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, Vector2 localMouse, bool isClicking) {
    mapsState.playerPos = (Vector2){currentPlayerPos.x, currentPlayerPos.y}; 
    
    // --- 1. FOLLOW LOGIC ---
    if (mapsState.isFollowingPlayer && !mapsState.isDragging) {
        Vector2 diff = Vector2Subtract(mapsState.playerPos, mapsState.camera.target);
        if (Vector2Length(diff) > 0.1f) {
            mapsState.camera.target = Vector2Add(mapsState.camera.target, Vector2Scale(diff, 0.1f));
        }
    }

    // --- 2. MOUSE INTERACTIONS ---
    if (isClicking) {
        // UI CLICK
        if (localMouse.y <= 80) {
             if (localMouse.x < 310 && localMouse.y > 30) {
                 mapsState.isSearching = true;
                 if (mapsState.searchCharCount == 0) ShowRecommendedPlaces();
             }
             else if (localMouse.x >= 310 && localMouse.y > 30) {
                 ResetMapCamera(mapsState.playerPos);
             }
        }
        // RE-CENTER BUTTON
        else if (localMouse.x > 340 && localMouse.y > 540) {
            mapsState.isFollowingPlayer = true;
        }
        // MAP CLICK
        else {
            float currentTime = GetTime();
            if ((currentTime - mapsState.lastClickTime) < 0.3f) {
                // --- DOUBLE CLICK LOGIC ---
                
                // 1. Calculate World Position of Click
                Vector2 rel = Vector2Subtract(localMouse, mapsState.camera.offset);
                Vector2 worldPos = Vector2Add(mapsState.camera.target, Vector2Scale(rel, 1.0f/mapsState.camera.zoom));
                
                // 2. SNAP TO ROAD (Threshold 15.0f is fairly wide/generous)
                Vector2 snappedPos = SnapToRoad(map, worldPos, 15.0f);
                
                SetMapDestination(map, snappedPos);
            }
            mapsState.lastClickTime = currentTime;

            mapsState.isDragging = true;
            mapsState.dragStart = localMouse; 
            mapsState.isSearching = false;    
            mapsState.isFollowingPlayer = false; 
        }
        
        // SEARCH RESULT CLICK
        if (mapsState.isSearching) {
            for(int i=0; i<mapsState.resultCount; i++) {
                Rectangle resRect = {10, 85 + i*45, 300, 40};
                if (CheckCollisionPointRec(localMouse, resRect)) {
                    SetMapDestination(map, mapsState.searchResults[i].position);
                    mapsState.isSearching = false;
                }
            }
        }
    }
    
    // --- 3. DRAGGING ---
    if (mapsState.isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 delta = Vector2Subtract(mapsState.dragStart, localMouse);
            Vector2 worldDelta = Vector2Scale(delta, 1.0f / mapsState.camera.zoom);
            mapsState.camera.target = Vector2Add(mapsState.camera.target, worldDelta);
            mapsState.dragStart = localMouse;
        } else {
            mapsState.isDragging = false;
        }
    }
    
    // --- 4. ZOOM ---
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        mapsState.camera.zoom += wheel * 0.5f; 
        if (mapsState.camera.zoom < 0.5f) mapsState.camera.zoom = 0.5f;
        if (mapsState.camera.zoom > 10.0f) mapsState.camera.zoom = 10.0f;
    }

    // --- 5. KEYBOARD ---
    if (mapsState.isSearching) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (mapsState.searchCharCount < 63)) {
                mapsState.searchQuery[mapsState.searchCharCount] = (char)key;
                mapsState.searchQuery[mapsState.searchCharCount + 1] = '\0';
                mapsState.searchCharCount++;
                mapsState.resultCount = SearchLocations(mapsState.searchQuery, mapsState.searchResults);
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (mapsState.searchCharCount > 0) {
                mapsState.searchCharCount--;
                mapsState.searchQuery[mapsState.searchCharCount] = '\0';
                if (mapsState.searchCharCount == 0) ShowRecommendedPlaces();
                else mapsState.resultCount = SearchLocations(mapsState.searchQuery, mapsState.searchResults);
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
    
    // Draw Roads 
    float lineThick = 4.0f / mapsState.camera.zoom; 
    if (lineThick < 1.0f) lineThick = 1.0f;

    for (int i = 0; i < map->edgeCount; i++) {
        Vector2 s = map->nodes[map->edges[i].startNode].position;
        Vector2 e = map->nodes[map->edges[i].endNode].position;
        DrawLineEx(s, e, map->edges[i].width, LIGHTGRAY);
    }
    
    // Draw Buildings
    for (int i = 0; i < map->buildingCount; i++) {
        DrawTriangleFan(map->buildings[i].footprint, map->buildings[i].pointCount, map->buildings[i].color);
    }

    // Draw Player
    DrawCircleV(mapsState.playerPos, 1.0f, BLUE);
    DrawCircleLines(mapsState.playerPos.x, mapsState.playerPos.y, 1.5f, SKYBLUE);

    // Draw Path
    if (mapsState.hasDestination && mapsState.pathLen > 1) {
        DrawLineStrip(mapsState.path, mapsState.pathLen, RED);
        
        // Draw connection from last path node to actual pin (Important visual fix)
        // because pin is mid-road, but path ends at intersection
        DrawLineEx(mapsState.path[mapsState.pathLen-1], mapsState.destination, lineThick, RED);
        
        // Pin
        DrawCircleV(mapsState.destination, 3.0f / mapsState.camera.zoom, RED);
        DrawCircleLines(mapsState.destination.x, mapsState.destination.y, 3.0f / mapsState.camera.zoom, BLACK);
    }

    EndMode2D();

    // --- UI OVERLAY ---
    
    DrawRectangle(0, 0, 400, 80, WHITE);
    DrawText("Maps", 10, 10, 20, BLACK);

    // Search Box
    DrawRectangle(10, 40, 300, 30, LIGHTGRAY);
    if (mapsState.isSearching) {
        DrawText(mapsState.searchQuery, 15, 48, 10, BLACK);
        if ((GetTime() * 2.0f) - (int)(GetTime() * 2.0f) < 0.5f) {
            DrawRectangle(15 + MeasureText(mapsState.searchQuery, 10), 48, 2, 16, BLACK);
        }
    } else {
        DrawText("Search here...", 15, 48, 10, GRAY);
    }

    // Reset Button
    DrawRectangle(320, 40, 70, 30, BLACK);
    DrawText("RESET", 335, 50, 10, WHITE);

    // Results Dropdown
    if (mapsState.isSearching && mapsState.resultCount > 0) {
        DrawRectangle(10, 80, 300, mapsState.resultCount * 45, WHITE);
        for(int i=0; i<mapsState.resultCount; i++) {
            float yPos = 85 + i*45;
            // Hover effect
            Rectangle itemRect = {10, 80 + i*45, 300, 45};
            if (CheckCollisionPointRec(GetMousePosition(), itemRect)) { 
                DrawRectangleRec(itemRect, Fade(SKYBLUE, 0.3f));
            }
            
            DrawText(mapsState.searchResults[i].name, 20, yPos, 20, BLACK);
            DrawText("Location", 20, yPos + 20, 10, GRAY);
            DrawLine(10, yPos + 40, 310, yPos + 40, LIGHTGRAY);
        }
    }

    // Re-Center Floating Button
    if (!mapsState.isFollowingPlayer) {
        DrawCircle(360, 560, 25, BLACK);
        DrawCircleLines(360, 560, 25, WHITE);
        DrawText("O", 355, 552, 20, WHITE); 
    }
}
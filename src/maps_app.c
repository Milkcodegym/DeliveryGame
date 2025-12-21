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
    bool isFollowingPlayer; 

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
    bool found = false;

    for (int i = 0; i < map->edgeCount; i++) {
        Vector2 start = map->nodes[map->edges[i].startNode].position;
        Vector2 end = map->nodes[map->edges[i].endNode].position;

        // AABB Check
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
            found = true;
        }
    }
    return bestPoint;
}

void ShowRecommendedPlaces(GameMap *map) {
    mapsState.resultCount = SearchLocations(map, "a", mapsState.searchResults);
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

// Robust SetMapDestination
void SetMapDestination(GameMap *map, Vector2 dest) {
    // Attempt pathfinding
    int len = FindPath(map, mapsState.playerPos, dest, mapsState.path, MAX_PATH_NODES);
    
    // Check if pathfinding failed
    if (len == 0) {
        float dist = Vector2Distance(mapsState.playerPos, dest);
        if (dist < 10.0f) {
            printf("MapsApp: Target is too close (Already arrived).\n");
            // Treat as success but no path needed
            mapsState.destination = dest;
            mapsState.hasDestination = false; // Don't draw path
            return;
        }
        printf("MapsApp: Could not find path to target. (Graph disconnected?)\n");
        return; 
    }

    // Success
    mapsState.destination = dest;
    mapsState.hasDestination = true;
    mapsState.pathLen = len;
    mapsState.isFollowingPlayer = true; // Auto-follow on new route
}

void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, Vector2 localMouse, bool isClicking) {
    mapsState.playerPos = (Vector2){currentPlayerPos.x, currentPlayerPos.y}; 
    
    // --- 1. END NAVIGATION (Close Proximity Check) ---
    if (mapsState.hasDestination) {
        // Reduced distance to 5.0f so you get closer before it disappears
        if (Vector2Distance(mapsState.playerPos, mapsState.destination) < 5.0f) {
            mapsState.hasDestination = false;
            mapsState.pathLen = 0;
        }
    }

    // --- 2. FOLLOW LOGIC ---
    if (mapsState.isFollowingPlayer && !mapsState.isDragging) {
        Vector2 diff = Vector2Subtract(mapsState.playerPos, mapsState.camera.target);
        if (Vector2Length(diff) > 0.1f) {
            mapsState.camera.target = Vector2Add(mapsState.camera.target, Vector2Scale(diff, 0.1f));
        }
    }

    // --- 3. MOUSE INTERACTIONS ---
    if (isClicking) {
        bool handled = false;

        // A. SEARCH RESULTS CLICK 
        if (mapsState.isSearching) {
            for(int i=0; i<mapsState.resultCount; i++) {
                Rectangle resRect = {10, 85 + i*45, 300, 40};
                if (CheckCollisionPointRec(localMouse, resRect)) {
                    SetMapDestination(map, mapsState.searchResults[i].position);
                    mapsState.isSearching = false;
                    handled = true;
                    break;
                }
            }
        }

        if (!handled) {
            // B. TOP UI CLICK 
            if (localMouse.y <= 80) {
                 if (localMouse.x < 310 && localMouse.y > 30) {
                     mapsState.isSearching = true;
                     if (mapsState.searchCharCount == 0) ShowRecommendedPlaces(map);
                 }
                 else if (localMouse.x >= 310 && localMouse.y > 30) {
                     ResetMapCamera(mapsState.playerPos);
                 }
            }
            // C. RE-CENTER BUTTON CLICK (Hitbox: 325-375, 425-475)
            else if (localMouse.x > 325 && localMouse.x < 375 && localMouse.y > 425 && localMouse.y < 475) {
                mapsState.isFollowingPlayer = true;
            }
            // D. MAP CLICK
            else {
                float currentTime = GetTime();
                // Double Click Window 0.5s
                if ((currentTime - mapsState.lastClickTime) < 0.5f) {
                    Vector2 rel = Vector2Subtract(localMouse, mapsState.camera.offset);
                    Vector2 worldPos = Vector2Add(mapsState.camera.target, Vector2Scale(rel, 1.0f/mapsState.camera.zoom));
                    
                    // Snap Threshold 30.0f
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
            Vector2 delta = Vector2Subtract(mapsState.dragStart, localMouse);
            Vector2 worldDelta = Vector2Scale(delta, 1.0f / mapsState.camera.zoom);
            mapsState.camera.target = Vector2Add(mapsState.camera.target, worldDelta);
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
    
    // Draw Roads 
    float lineThick = 5.0f / mapsState.camera.zoom; 
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

    // Draw Player (Larger Icon)
    DrawCircleV(mapsState.playerPos, 3.0f, BLUE);
    DrawCircleLines(mapsState.playerPos.x, mapsState.playerPos.y, 3.5f, SKYBLUE);

    // Draw Path (THICK LINES)
    if (mapsState.hasDestination && mapsState.pathLen > 1) {
        float pathThick = 8.0f / mapsState.camera.zoom; 
        for (int i = 0; i < mapsState.pathLen - 1; i++) {
            DrawLineEx(mapsState.path[i], mapsState.path[i+1], pathThick, RED);
        }
        DrawLineEx(mapsState.path[mapsState.pathLen-1], mapsState.destination, pathThick, RED);
        
        // Pin
        DrawCircleV(mapsState.destination, 4.0f / mapsState.camera.zoom, RED);
        DrawCircleLines(mapsState.destination.x, mapsState.destination.y, 4.0f / mapsState.camera.zoom, BLACK);
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

    // --- RE-CENTER FLOATING BUTTON (ALWAYS VISIBLE) ---
    // Position: 350, 450 (Higher up to ensure visibility)
    // GREEN = Locked/Following
    // GRAY  = Unlocked/Free Camera
    Color btnColor = mapsState.isFollowingPlayer ? GREEN : LIGHTGRAY;
    Color iconColor = mapsState.isFollowingPlayer ? WHITE : BLACK;
    
    DrawCircle(350, 450, 25, btnColor);
    DrawCircleLines(350, 450, 25, BLACK);
    DrawText("O", 345, 442, 20, iconColor); 
}
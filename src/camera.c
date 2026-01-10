#include "camera.h"
#include <math.h>
#include "raymath.h"

Camera3D camera = { 0 };

// Variables to store smooth states
static float smoothedCollisionDist = 3.2f;
static Vector3 smoothedTarget = { 0 };

void InitCamera(){
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = (Vector3){ 0.0f, 10.0f, -10.0f };
    smoothedTarget = (Vector3){0,0,0};
}

void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt){
    // Initialize target on first run to prevent flying from 0,0,0
    if (smoothedTarget.y == 0) smoothedTarget = player_position;

    // --- 1. SMOOTH TARGET (Fixes the "Vibrating Look" issue) ---
    // Instead of locking eyes on the player instantly, we lag behind slightly.
    // This filters out micro-jitters from the player physics.
    Vector3 idealTarget = { player_position.x, player_position.y + 0.5f, player_position.z };
    
    // Lerp the target point (High speed = minimal lag, but enough to smooth jitter)
    float targetSmoothSpeed = 15.0f; 
    smoothedTarget = Vector3Lerp(smoothedTarget, idealTarget, targetSmoothSpeed * dt);
    
    camera.target = smoothedTarget;

    // --- 2. CALCULATE POSITION ---
    float maxCamDist = 3.2f;       
    float camHeight = 1.5f;     

    Vector3 targetOffset;
    targetOffset.x = -maxCamDist * sinf(player_angle * DEG2RAD);
    targetOffset.z = -maxCamDist * cosf(player_angle * DEG2RAD);
    targetOffset.y = camHeight;

    // --- 3. COLLISION CHECK ---
    float currentDist = maxCamDist; 
    float dx = targetOffset.x;
    float dz = targetOffset.z;
    
    int steps = 8; 
    for (int i = 0; i <= steps; i++) {
        float t = 0.2f + ((float)i / steps) * 0.8f;
        float checkX = player_position.x + (dx * t);
        float checkZ = player_position.z + (dz * t);

        if (CheckMapCollision(map, checkX, checkZ, 0.3f, 1)) {
            currentDist = (t * maxCamDist) - 0.2f;
            if (currentDist < 0.5f) currentDist = 0.5f;
            break; 
        }
    }

    // --- 4. SMOOTH DISTANCE ---
    // Move IN fast (to avoid wall clipping), move OUT slow
    float zoomSpeed = (currentDist < smoothedCollisionDist) ? 15.0f : 2.0f;
    smoothedCollisionDist = Lerp(smoothedCollisionDist, currentDist, zoomSpeed * dt);

    // --- 5. APPLY POSITION ---
    // Calculate final position based on the SMOOTHED target and SMOOTHED distance
    // This ensures the camera body and camera eye move in sync.
    Vector3 finalPos;
    finalPos.x = smoothedTarget.x - smoothedCollisionDist * sinf(player_angle * DEG2RAD);
    finalPos.z = smoothedTarget.z - smoothedCollisionDist * cosf(player_angle * DEG2RAD);
    finalPos.y = smoothedTarget.y + camHeight; // Base height off target, not raw player

    // Wall lift
    if (smoothedCollisionDist < 1.5f) {
        finalPos.y += (1.5f - smoothedCollisionDist) * 0.5f;
    }

    // Final position smoothing
    camera.position = Vector3Lerp(camera.position, finalPos, 8.0f * dt);
}
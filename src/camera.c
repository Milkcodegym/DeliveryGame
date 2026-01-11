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
    // [FIX 1] Game Time Clamping (The "Player Physics" Approach)
    // We use Delta Time so the camera speed is consistent regardless of FPS.
    // But we CAP it at 0.04 (25 FPS) so lag spikes don't cause explosions.
    float safeDt = (dt > 0.04f) ? 0.04f : dt;

    // Reset logic (Teleport if too far)
    float distToPlayer = Vector3Distance(camera.position, player_position);
    if (smoothedTarget.y == 0 || distToPlayer > 50.0f) {
        smoothedTarget = player_position;
        camera.position.x = player_position.x - 5.0f;
        camera.position.z = player_position.z - 5.0f;
        camera.position.y = player_position.y + 5.0f;
        smoothedCollisionDist = 3.2f;
    }

    // --- 1. SMOOTH TARGET (Time-Based) ---
    Vector3 idealTarget = { player_position.x, player_position.y + 0.5f, player_position.z };
    
    // Use Lerp with safeDt. 
    // Speed 10.0f means it tries to cover the distance in ~0.1 seconds.
    float targetSpeed = 10.0f;
    smoothedTarget = Vector3Lerp(smoothedTarget, idealTarget, targetSpeed * safeDt);

    // [FIX 2] Target Tether (Hard Constraint)
    // If the smoothed target falls more than 2.5m behind, force it closer.
    // This fixes the "drifting away when accelerating" issue.
    float targetLag = Vector3Distance(smoothedTarget, idealTarget);
    if (targetLag > 2.5f) {
        Vector3 dir = Vector3Normalize(Vector3Subtract(smoothedTarget, idealTarget));
        smoothedTarget = Vector3Add(idealTarget, Vector3Scale(dir, 2.5f));
    }
    
    camera.target = smoothedTarget;

    // --- 2. CALCULATE OFFSET ---
    float maxCamDist = 2.4f;       
    float camHeight = 0.9f;     

    Vector3 targetOffset;
    targetOffset.x = -maxCamDist * sinf(player_angle * DEG2RAD);
    targetOffset.z = -maxCamDist * cosf(player_angle * DEG2RAD);
    targetOffset.y = camHeight;

    // --- 3. COLLISION CHECK ---
    float currentDist = maxCamDist; 
    float dx = targetOffset.x;
    float dz = targetOffset.z;
    
    // 4 steps is enough for a smooth camera
    int steps = 4; 
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

    // --- 4. SMOOTH DISTANCE (Time-Based) ---
    float zoomSpeed = (currentDist < smoothedCollisionDist) ? 15.0f : 3.0f; // Fast in, Slow out
    smoothedCollisionDist = Lerp(smoothedCollisionDist, currentDist, zoomSpeed * safeDt);

    // [FIX 3] Max Zoom Constraint
    // Camera cannot be further back than 4.5m, no matter what smoothing says.
    if (smoothedCollisionDist > 3.1f) smoothedCollisionDist = 3.1f;

    // --- 5. APPLY POSITION ---
    Vector3 finalPos;
    finalPos.x = smoothedTarget.x - smoothedCollisionDist * sinf(player_angle * DEG2RAD);
    finalPos.z = smoothedTarget.z - smoothedCollisionDist * cosf(player_angle * DEG2RAD);
    finalPos.y = smoothedTarget.y + camHeight; 

    // Wall lift
    if (smoothedCollisionDist < 1.5f) {
        finalPos.y += (1.5f - smoothedCollisionDist) * 0.5f;
    }

    // [FIX 4] Height Constraints
    if (finalPos.y < 0.5f) finalPos.y = 0.5f;   // Floor clamp
    if (finalPos.y > 20.0f) finalPos.y = 20.0f; // Ceiling clamp

    // Final Position Smoothing (Time-Based)
    camera.position = Vector3Lerp(camera.position, finalPos, 8.0f * safeDt);
    
    // Final Safety Clamp
    if (camera.position.y < 0.5f) camera.position.y = 0.5f;
}
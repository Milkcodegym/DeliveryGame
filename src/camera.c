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

#include "camera.h"
#include <math.h>
#include "raymath.h"

Camera3D camera = { 0 };

// Variables to store smooth states
static float smoothedCollisionDist = 3.2f;
static Vector3 smoothedTarget = { 0 };

/*
 * Description: Initializes the 3D camera with default values.
 * Parameters: None.
 * Returns: None.
 */
void InitCamera() {
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = (Vector3){ 0.0f, 10.0f, -10.0f };
    smoothedTarget = (Vector3){0, 0, 0};
}

/*
 * Description: Updates the camera position and target based on the player's movement, handling smoothing and collision.
 * Parameters:
 * - player_position: The current 3D position of the player.
 * - map: Pointer to the game map for collision checks.
 * - player_angle: The facing angle of the player in degrees.
 * - dt: Delta time for frame-rate independent movement.
 * Returns: None.
 */
void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt) {
    // Clamp Delta Time to avoid instability during lag spikes (max 25 FPS step)
    float safeDt = (dt > 0.04f) ? 0.04f : dt;

    // Reset logic (Teleport if too far or uninitialized)
    float distToPlayer = Vector3Distance(camera.position, player_position);
    if (smoothedTarget.y == 0 || distToPlayer > 50.0f) {
        smoothedTarget = player_position;
        camera.position.x = player_position.x - 5.0f;
        camera.position.z = player_position.z - 5.0f;
        camera.position.y = player_position.y + 5.0f;
        smoothedCollisionDist = 3.2f;
    }

    // --- 1. SMOOTH TARGET ---
    Vector3 idealTarget = { player_position.x, player_position.y + 0.5f, player_position.z };
    
    // Time-based Lerp
    float targetSpeed = 10.0f;
    smoothedTarget = Vector3Lerp(smoothedTarget, idealTarget, targetSpeed * safeDt);

    // Target Tether (Hard Constraint)
    // If the smoothed target lags too far behind, force it closer
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

    // --- 4. SMOOTH DISTANCE ---
    float zoomSpeed = (currentDist < smoothedCollisionDist) ? 15.0f : 3.0f; // Fast in, Slow out
    smoothedCollisionDist = Lerp(smoothedCollisionDist, currentDist, zoomSpeed * safeDt);

    // Max Zoom Constraint
    if (smoothedCollisionDist > 3.1f) smoothedCollisionDist = 3.1f;

    // --- 5. APPLY POSITION ---
    Vector3 finalPos;
    finalPos.x = smoothedTarget.x - smoothedCollisionDist * sinf(player_angle * DEG2RAD);
    finalPos.z = smoothedTarget.z - smoothedCollisionDist * cosf(player_angle * DEG2RAD);
    finalPos.y = smoothedTarget.y + camHeight; 

    // Wall lift effect (raise camera if too close)
    if (smoothedCollisionDist < 1.5f) {
        finalPos.y += (1.5f - smoothedCollisionDist) * 0.5f;
    }

    // Height Constraints
    if (finalPos.y < 0.5f) finalPos.y = 0.5f;   
    if (finalPos.y > 20.0f) finalPos.y = 20.0f; 

    // Final Position Smoothing
    camera.position = Vector3Lerp(camera.position, finalPos, 8.0f * safeDt);
    
    // Final Safety Clamp
    if (camera.position.y < 0.5f) camera.position.y = 0.5f;
}
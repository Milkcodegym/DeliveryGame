#include "camera.h"
#include <math.h>
#include "raymath.h"

Camera3D camera = { 0 };

void InitCamera(){
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    // Set a dummy position so it doesn't start at 0,0,0 if player is elsewhere
    camera.position = (Vector3){ 0.0f, 10.0f, -10.0f };
}

void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt){
    // 1. Target Position
    float camDist = 7.0f;       
    float camHeight = 3.5f;     

    Vector3 desiredPos;
    desiredPos.x = player_position.x - camDist * sinf(player_angle * DEG2RAD);
    desiredPos.z = player_position.z - camDist * cosf(player_angle * DEG2RAD);
    desiredPos.y = player_position.y + camHeight;

    // Instant Teleport logic (Keep existing)
    if (Vector3Distance(camera.position, player_position) > 100.0f) {
        camera.position = desiredPos;
    }

    // 2. LOGIC FIX: High-Precision Raycast
    if (checkcamera_collision) {
        float dx = desiredPos.x - player_position.x;
        float dz = desiredPos.z - player_position.z;
        float dist = sqrtf(dx*dx + dz*dz); // Calculate total distance
        
        // DYNAMIC STEPS: We want a check every 20cm (0.2 units)
        // If distance is 7m, we need ~35 checks, not 8.
        int steps = (int)(dist / 0.2f); 
        if (steps < 5) steps = 5; // Safety minimum

        // Start at 10% (0.1) to skip the car's own body
        for (int i = 0; i <= steps; i++) {
            float t = 0.1f + ((float)i / steps) * 0.9f;
            
            float checkX = player_position.x + (dx * t);
            float checkZ = player_position.z + (dz * t);

            // HIT CHECK: Use 0.4f Radius (Fatter check)
            // This stops the camera BEFORE it touches the wall visuals
            if (CheckMapCollision(map, checkX, checkZ, 0.4f)) {
                
                // PULL BACK:
                // If we hit at 't', we want to position the camera slightly 
                // BEFORE that point to provide a safety buffer.
                // Pull back by 0.5 meters (relative to total dist)
                float buffer = 0.5f / dist; 
                float safeT = fmaxf(t - buffer, 0.1f); 
                
                desiredPos.x = player_position.x + (dx * safeT);
                desiredPos.z = player_position.z + (dz * safeT);
                
                // Lift camera slightly when tight against wall to see better
                desiredPos.y += 1.5f * (1.0f - safeT); 
                break; // Stop checking, we hit the closest wall
            }
        }
    }

    // 3. Smooth Movement (Lerp)
    float smoothSpeed = 10.0f; // Slightly faster smoothing so it doesn't lag inside walls
    camera.position.x += (desiredPos.x - camera.position.x) * smoothSpeed * dt;
    camera.position.z += (desiredPos.z - camera.position.z) * smoothSpeed * dt;
    camera.position.y += (desiredPos.y - camera.position.y) * smoothSpeed * dt;

    camera.target = (Vector3){ player_position.x, player_position.y + 0.5f, player_position.z };
}
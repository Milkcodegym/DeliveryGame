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
    // 1. Calculate the Ideal "Perfect" Position (Max Distance)
    float camDist = 6.0f;       // Increased slightly for better view of city
    float camHeight = 2.5f;     // Increased height to see over traffic

    // Where we WANT the camera to be
    Vector3 desiredPos;
    desiredPos.x = player_position.x - camDist * sinf(player_angle * DEG2RAD);
    desiredPos.z = player_position.z - camDist * cosf(player_angle * DEG2RAD);
    desiredPos.y = player_position.y + camHeight;

    // [FIX 1] Instant Teleport on Start
    // If camera is > 100 units away (e.g. at 0,0 while player is at 2000,2000), snap it.
    if (Vector3Distance(camera.position, player_position) > 100.0f) {
        camera.position = desiredPos;
    }

    // 2. Collision Check: Raycast from Player TO Camera
    // Checks after W is pressed for first time
    if (checkcamera_collision) {
        float dx = desiredPos.x - player_position.x;
        float dz = desiredPos.z - player_position.z;
        
        int steps = 8; // Optimized steps
        
        // [FIX 2] Start loop at 2 instead of 1.
        // This ignores collisions in the first 20% of the distance (the player's car body).
        for (int i = 2; i <= steps; i++) {
            float t = (float)i / steps;
            
            float checkX = player_position.x + (dx * t);
            float checkZ = player_position.z + (dz * t);

            // Using slightly smaller radius (0.1f) for camera ray to avoid snagging on light poles
            if (CheckMapCollision(map, checkX, checkZ, 0.1f)) {
                // WALL HIT! 
                // [FIX 3] Safety Buffer Calculation
                // Don't pull closer than 20% (t=0.2) or it clips into car
                float safeT = fmaxf(t - 0.1f, 0.2f); 
                
                desiredPos.x = player_position.x + (dx * safeT);
                desiredPos.z = player_position.z + (dz * safeT);
                // Lift camera up if compressed against wall
                desiredPos.y += 1.0f * (1.0f - safeT); 
                break; 
            }
        }
    }

    // 3. Smoothly move Camera
    float smoothSpeed = 8.0f; // Snappier response
    camera.position.x += (desiredPos.x - camera.position.x) * smoothSpeed * dt;
    camera.position.z += (desiredPos.z - camera.position.z) * smoothSpeed * dt;
    camera.position.y += (desiredPos.y - camera.position.y) * smoothSpeed * dt;

    // 4. Look at player (with slight offset up)
    camera.target = (Vector3){ player_position.x, player_position.y + 0.5f, player_position.z };
}
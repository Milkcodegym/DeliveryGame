#include "camera.h"
#include <math.h>

Camera3D camera = { 0 };

void InitCamera(){
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt){
    // 1. Calculate the Ideal "Perfect" Position (Max Distance)
    float camDist = 3.0f;
    float camHeight = 0.8f;

    // Where we WANT the camera to be
    Vector3 desiredPos;
    desiredPos.x = player_position.x - camDist * sinf(player_angle * DEG2RAD);
    desiredPos.z = player_position.z - camDist * cosf(player_angle * DEG2RAD);
    desiredPos.y = player_position.y + camHeight;

    // 2. Collision Check: Raycast from Player TO Camera
    // We step from the player towards the camera. If we hit a wall, we stop there.
    // Checks after W is pressed for first time
    if (checkcamera_collision) {
        float dx = desiredPos.x - player_position.x;
        float dz = desiredPos.z - player_position.z;
        
        // Number of checks between player and camera (higher = smoother, lower = faster)
        int steps = 10; 
        
        for (int i = 1; i <= steps; i++) {
            float t = (float)i / steps;
            
            // Calculate a point 't' percent of the way to the camera
            float checkX = player_position.x + (dx * t);
            float checkZ = player_position.z + (dz * t);

            if (CheckMapCollision(map, checkX, checkZ)) {
                // WALL HIT! 
                // Place camera slightly closer to player than the wall (buffer) to avoid clipping
                float buffer = 0.2f; 
                desiredPos.x = player_position.x + (dx * (t - buffer/camDist));
                desiredPos.z = player_position.z + (dz * (t - buffer/camDist));
                break; // Stop checking, we found the wall
            }
        }
    }

    // 3. Smoothly move Camera to the safe spot (Lerp)
    // We apply smoothing to the FINAL calculated position, not the raw movement
    float smoothSpeed = 5.0f;
    camera.position.x += (desiredPos.x - camera.position.x) * smoothSpeed * dt;
    camera.position.z += (desiredPos.z - camera.position.z) * smoothSpeed * dt;
    camera.position.y += (desiredPos.y - camera.position.y) * smoothSpeed * dt;

    // 4. Look at player
    camera.target = player_position;
}

#include "camera.h"
#include <math.h>

Camera3D camera = { 0 };

void InitCamera(){
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void Update_Camera(Vector3 player_position, float player_angle, float dt){
    // Camera Follow Logic
    // Calculate the camera position behind the player based on angle
    float camDist = 3.0f;
    float camHeight = 0.8f;
    
    Vector3 desiredPos;
    desiredPos.x = player_position.x - camDist * sinf(player_angle * DEG2RAD);
    desiredPos.z = player_position.z - camDist * cosf(player_angle * DEG2RAD);
    desiredPos.y = player_position.y + camHeight;

    // Simple smooth follow (Lerp)
    camera.position.x += (desiredPos.x - camera.position.x) * 5.0f * dt;
    camera.position.z += (desiredPos.z - camera.position.z) * 5.0f * dt;
    camera.position.y += (desiredPos.y - camera.position.y) * 5.0f * dt;
    
    camera.target = player_position;

    
}

void Begin3Dmode(){BeginMode3D(camera);}
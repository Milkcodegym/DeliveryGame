#include "player.h"
#include <math.h>

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    p.speed = 4.0f;
    p.radius = 0.3f;
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    p.angle = 0.0f;
    return p;
}

void UpdatePlayer(Player *player, GameMap *map, float dt) {
    Vector3 move = { 0 };
    float rotationSpeed = 90.0f;

    // --- 1. Rotation ---
    if (IsKeyDown(KEY_A)) player->angle += rotationSpeed * dt;
    if (IsKeyDown(KEY_D)) player->angle -= rotationSpeed * dt;

    // --- 2. Calculate Movement Vector (But don't apply yet!) ---
    if (IsKeyDown(KEY_W)) {
        move.x += sinf(player->angle * DEG2RAD);
        move.z += cosf(player->angle * DEG2RAD);
    }
    if (IsKeyDown(KEY_S)) {
        move.x -= sinf(player->angle * DEG2RAD);
        move.z -= cosf(player->angle * DEG2RAD);
    }

    // Normalize and Scale by Speed
    if (move.x != 0 || move.z != 0) {
        float length = sqrtf(move.x*move.x + move.z*move.z);
        move.x /= length; 
        move.z /= length;
        
        move.x *= player->speed * dt;
        move.z *= player->speed * dt;
    }

    // --- 3. Gravity (Y Axis) ---
    // (This works fine as is)
    player->yVelocity -= 20.0f * dt;
    if (player->isGrounded && IsKeyPressed(KEY_SPACE)) player->yVelocity = 8.0f;
    
    player->position.y += player->yVelocity * dt;

    // Floor Collision
    if (player->position.y <= player->radius) {
        player->position.y = player->radius;
        player->yVelocity = 0;
        player->isGrounded = true;
    } else {
        player->isGrounded = false;
    }

    // --- 4. Map Collision & Movement Application ---
    // We only update player->position IF the new spot is safe.
    
    if (map->width > 0) {
        // Check X movement
        if (!CheckMapCollision(map, player->position.x + move.x, player->position.z, player->radius)) {
            player->position.x += move.x;
        }

        // Check Z movement
        if (!CheckMapCollision(map, player->position.x, player->position.z + move.z, player->radius)) {
            player->position.z += move.z;
        }
    }
}


void DrawPlayer(Player *player) {
    //DrawSphere(player->position, player->radius, YELLOW);
    player->model = LoadModel("resources/vehicle-suv.obj");
}
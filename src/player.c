#include "player.h"
#include <math.h>
#include <stdio.h>

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    p.current_speed = 0.0f;
    p.friction = 7.0f;    //Needs to be over 1
    p.acceleration = 1.4f;
    p.max_speed = 8.0f;
    p.brake_power = 9.0f; //Needs to be over 1
    p.rotationSpeed = 90.0f;
    p.radius = 0.3f;
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    p.angle = 0.0f;
    return p;
}

// Renamed from DrawPlayer to reflect what it actually does
void LoadPlayerContent(Player *player) {
    player->model = LoadModel("resources/vehicle-suv.obj");
}

void UpdatePlayer(Player *player, GameMap *map, float dt) {
    // --- SAFETY FIX: Clamp Delta Time ---
    // If the game lags (or just started), dt might be huge (e.g. 5.0s).
    // We cap it at 0.1s so physics doesn't explode.
    if (dt > 0.1f) dt = 0.1f;

    Vector3 move = { 0 };

    // --- 1. Rotation ---
    if (IsKeyDown(KEY_A)) player->angle += player->rotationSpeed * dt;
    if (IsKeyDown(KEY_D)) player->angle -= player->rotationSpeed * dt;

    // 1. Determine Target Speed
    float target_speed = 0.0f;

    if (IsKeyDown(KEY_W)) target_speed = player->max_speed;
    if (IsKeyDown(KEY_S)) target_speed = -player->max_speed;

    if (player->current_speed < target_speed) {
        // We need to speed up (or reverse from negative)
        player->current_speed += player->acceleration * dt;
        
        // Cap it so we don't overshoot the target
        if (player->current_speed > target_speed) player->current_speed = target_speed;
        if ((player->current_speed<0)&&(target_speed>0)) player->current_speed += player->acceleration * dt * (player->brake_power-1);
        if (target_speed=0) player->current_speed += player->acceleration * dt * (player->friction-1);
    } 
    else if (player->current_speed > target_speed) {
        // We need to slow down (or reverse from positive)
        player->current_speed -= player->acceleration * dt;
        
        // Cap it so we don't overshoot the target
        if (player->current_speed < target_speed) player->current_speed = target_speed;
        if ((player->current_speed>0)&&(target_speed<0)) player->current_speed -= player->acceleration * dt * (player->brake_power-1);
        if (target_speed=0) player->current_speed -= player->acceleration * dt * (player->friction-1);
    }

    // 3. Apply Movement
    move.x += sinf(player->angle * DEG2RAD) * player->current_speed * dt;
    move.z += cosf(player->angle * DEG2RAD) * player->current_speed * dt;







    // --- 3. Gravity ---
    player->yVelocity -= 20.0f * dt;
    
    if (player->isGrounded && IsKeyPressed(KEY_SPACE)) {
        player->yVelocity = 8.0f;
        player->isGrounded = false; // Important to set false immediately
    }
    
    player->position.y += player->yVelocity * dt;

    // Floor Collision
    if (player->position.y <= player->radius) {
        player->position.y = player->radius;
        player->yVelocity = 0;
        player->isGrounded = true;
    } else {
        player->isGrounded = false;
    }

    // --- 4. Horizontal Collision ---
    if (!CheckMapCollision(map, player->position.x + move.x, player->position.z, player->radius)) {
        player->position.x += move.x;
    }
    if (!CheckMapCollision(map, player->position.x, player->position.z + move.z, player->radius)) {
        player->position.z += move.z;
    }
}
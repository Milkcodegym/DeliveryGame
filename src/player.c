#include "player.h"
#include <math.h>

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    p.speed = 6.0f;
    p.radius = 0.3f;
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    return p;
}

void UpdatePlayer(Player *player, GameMap *map, float dt) {
    // 1. Input
    Vector3 move = { 0 };
    if (IsKeyDown(KEY_W)) move.z -= 1.0f;
    if (IsKeyDown(KEY_S)) move.z += 1.0f;
    if (IsKeyDown(KEY_A)) move.x -= 1.0f;
    if (IsKeyDown(KEY_D)) move.x += 1.0f;

    if (move.x != 0 || move.z != 0) {
        float length = sqrtf(move.x*move.x + move.z*move.z);
        move.x /= length; move.z /= length;
        move.x *= player->speed * dt;
        move.z *= player->speed * dt;
    }

    // 2. Gravity
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

    // 3. Map Collision
    if (map->width > 0) {
        // Check X movement
        if (!CheckMapCollision(map, player->position.x + move.x, player->position.z)) {
            player->position.x += move.x;
        }
        // Check Z movement
        if (!CheckMapCollision(map, player->position.x, player->position.z + move.z)) {
            player->position.z += move.z;
        }
    }
}

void DrawPlayer(Player *player) {
    DrawSphere(player->position, player->radius, YELLOW);
}
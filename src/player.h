#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "map.h"

typedef struct Player {
    Vector3 position;
    float speed;
    float radius;
    float yVelocity;
    bool isGrounded;
} Player;

Player InitPlayer(Vector3 startPos);
void UpdatePlayer(Player *player, GameMap *map, float dt);
void DrawPlayer(Player *player);

#endif
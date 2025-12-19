#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "map.h"

typedef struct Player {
    Vector3 position;
    float current_speed;
    float max_speed;
    float radius;
    float rotationSpeed;
    float yVelocity;
    bool isGrounded;
    float angle;
    Model model;
    float acceleration;
    float brake_power;
    float friction;    
} Player;

Player InitPlayer(Vector3 startPos);
void LoadPlayerContent(Player *player); // Renamed
void UpdatePlayer(Player *player, GameMap *map, float dt);

#endif
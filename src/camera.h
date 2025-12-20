#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"
#include "map.h"
#include "player.h"

extern Camera3D camera;
void InitCamera();
void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt);

#endif
#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"
#include "map.h"
//#include "player.h"

void InitCamera();
void Update_Camera(Vector3 player_position, float player_angle, float dt);
void Begin3Dmode();

#endif
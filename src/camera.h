/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 *
 * License: zlib/libpng
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Full license terms: see the LICENSE file.
 * -----------------------------------------------------------------------------
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"
#include "map.h"
#include "player.h"

extern Camera3D camera;
void InitCamera();
void Update_Camera(Vector3 player_position, GameMap *map, float player_angle, float dt);

#endif
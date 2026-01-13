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


#ifndef MAPS_APP_H
#define MAPS_APP_H

#include "raylib.h"
#include "map.h"
#define MAX_PATH_NODES 2048


void InitMapsApp();
// Updated signature: Now accepts 'playerAngle'
void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, float playerAngle, Vector2 localMouse, bool isClicking);
void DrawMapsApp(GameMap *map);
void SetMapDestination(GameMap *map, Vector2 dest);
void PreviewMapLocation(GameMap *map, Vector2 target);
void ResetMapCamera(Vector2 playerPos);

bool IsMapsAppTyping();

#endif
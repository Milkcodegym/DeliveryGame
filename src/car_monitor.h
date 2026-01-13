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

#ifndef CAR_MONITOR_APP_H
#define CAR_MONITOR_APP_H

#include "raylib.h"
#include "player.h"

void DrawCarMonitorApp(Player *player, Vector2 localMouse, bool click);

#endif
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


#ifndef MECHANIC_UI_H
#define MECHANIC_UI_H

#include "raylib.h"
#include "player.h"
#include "phone.h"

// Draws interactive Mechanic UI. Returns true if window is active/open.
bool DrawMechanicWindow(Player *player, PhoneState *phone, bool isActive, int screenW, int screenH);

#endif
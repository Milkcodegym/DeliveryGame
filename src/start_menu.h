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

#ifndef START_MENU_H
#define START_MENU_H

#include "raylib.h"
#include "save.h"

void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status);

// Returns true if the user pressed start and we reached 50% (Time to load map)
// Returns false if the user closed the window
bool RunStartMenu_PreLoad(int screenWidth, int screenHeight);

// Call this at the END of your main game drawing loop.
// Returns TRUE if the loading screen is still active (keep drawing).
// Returns FALSE when it hits 100% and should stop being called.
bool DrawPostLoadOverlay(int screenWidth, int screenHeight, float dt);

#endif
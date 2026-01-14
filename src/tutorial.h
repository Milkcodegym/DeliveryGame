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

#ifndef TUTORIAL_H
#define TUTORIAL_H

#include "player.h"
#include "phone.h"
#include "save.h"
#include "maps_app.h"
#include "map.h"
#include "dealership.h" // Needed for DealershipState



void InitTutorial(void);

// [UPDATED] Now accepts UI states to track progress
bool UpdateTutorial(
    Player *player, 
    PhoneState *phone, 
    GameMap *map, 
    float dt,
    bool isRefueling,
    bool isMechanicOpen
);

void DrawTutorial(Player *player, PhoneState *phone, bool isRefueling);

// Helper to skip
void SkipTutorial(Player *player, PhoneState *phone);

#endif
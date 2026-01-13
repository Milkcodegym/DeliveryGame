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

#ifndef DEALERSHIP_H
#define DEALERSHIP_H

#include "raylib.h"
#include "player.h"
typedef struct Player Player;

// State management for the main game loop
typedef enum {
    DEALERSHIP_ACTIVE,
    DEALERSHIP_INACTIVE
} DealershipState;

// Initialize the dealership (load static room assets)
void InitDealership();

// Cleanup dealership (unload static room assets)
void UnloadDealershipSystem();

// Call this when the player triggers the entrance
void EnterDealership(Player *player);

// Call this when the player leaves (ESC)
void ExitDealership();

// Main logic loop for the dealership
void UpdateDealership(Player *player);

// Main render loop for the dealership
void DrawDealership(Player *player);

// Helper to check if player is near the door
bool CheckDealershipTrigger(Vector3 playerPos);

DealershipState GetDealershipState();
#endif
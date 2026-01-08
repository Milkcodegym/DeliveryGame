#ifndef TUTORIAL_H
#define TUTORIAL_H

#include "player.h"
#include "phone.h"
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

void DrawTutorial(Player *player, PhoneState *phone);

// Helper to skip
void SkipTutorial(Player *player, PhoneState *phone);

#endif
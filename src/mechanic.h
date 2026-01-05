#ifndef MECHANIC_UI_H
#define MECHANIC_UI_H

#include "raylib.h"
#include "player.h"
#include "phone.h"

// Draws interactive Mechanic UI. Returns true if window is active/open.
bool DrawMechanicWindow(Player *player, PhoneState *phone, bool isActive, int screenW, int screenH);

#endif
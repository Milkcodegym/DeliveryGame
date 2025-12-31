#ifndef SCREENVIS_H
#define SCREENVIS_H

#include "raylib.h"
#include "player.h"

// Draws standard driving HUD (Speed, WASD)
void DrawVisuals(float currentSpeed, float maxSpeed);

// [NEW] Draws fuel gauge and warnings
void DrawFuelOverlay(Player *player, int screenW, int screenH);

// [NEW] Draws interactive refueling UI. Returns true if window is active/open.
bool DrawRefuelWindow(Player *player, bool isActive, int screenW, int screenH);

#endif
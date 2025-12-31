#ifndef SCREENVIS_H
#define SCREENVIS_H

#include "raylib.h"
#include "player.h"

// Draws standard driving HUD (Speed, WASD)
void DrawVisuals(float currentSpeed, float maxSpeed);

// Draws fuel gauge and warnings
void DrawFuelOverlay(Player *player, int screenW, int screenH);

// Draws interactive refueling UI. Returns true if window is active/open.
bool DrawRefuelWindow(Player *player, bool isActive, int screenW, int screenH);

// [NEW] Visuals Update loop for timers (Dynamic Pricing)
void UpdateVisuals(float dt);

// Wrapper that calls standard visuals + pinned stats
void DrawVisualsWithPinned(Player *player);

#endif
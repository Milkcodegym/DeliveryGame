#ifndef MAPS_APP_H
#define MAPS_APP_H

#include "raylib.h"
#include "map.h"

void InitMapsApp();
void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, Vector2 localMouse, bool isClicking);
void DrawMapsApp(GameMap *map);
void SetMapDestination(GameMap *map, Vector2 dest);
void ResetMapCamera(Vector2 playerPos);

// New: Check if user is typing so we can disable car controls
bool IsMapsAppTyping();

#endif
#ifndef MAPS_APP_H
#define MAPS_APP_H

#include "raylib.h"
#include "map.h"
#define MAX_PATH_NODES 2048

void InitMapsApp();
// Updated signature: Now accepts 'playerAngle'
void UpdateMapsApp(GameMap *map, Vector2 currentPlayerPos, float playerAngle, Vector2 localMouse, bool isClicking);
void DrawMapsApp(GameMap *map);
void SetMapDestination(GameMap *map, Vector2 dest);
void ResetMapCamera(Vector2 playerPos);

bool IsMapsAppTyping();

#endif
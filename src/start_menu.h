#ifndef START_MENU_H
#define START_MENU_H

#include "raylib.h"
#include "map.h"

// The main menu loop
GameMap RunStartMenu(const char* mapFileName,int screenWidth,int screenHeight);

// NEW: Helper to draw the loading overlay
void DrawLoadingInterface(int screenW, int screenH, float progress, const char* status);

#endif
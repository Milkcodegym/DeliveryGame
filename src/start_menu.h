#ifndef START_MENU_H
#define START_MENU_H

#include "raylib.h"
#include "map.h" // Assuming you need GameMap definition

// Expose the loading bar function so main.c can use it
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status);

// The main menu entry point
GameMap RunStartMenu(const char* mapFileName, int screenWidth, int screenHeight);

#endif
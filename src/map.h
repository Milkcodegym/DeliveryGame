#ifndef MAP_H
#define MAP_H

#include "raylib.h"

typedef struct GameMap {
    Image image;
    Color *pixels;
    int width;
    int height;
} GameMap;

GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map);
bool CheckMapCollision(GameMap *map, float x, float z);

#endif
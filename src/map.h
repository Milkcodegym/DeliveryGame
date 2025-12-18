#ifndef MAP_H
#define MAP_H

#include "raylib.h"

typedef struct RoadTile {
    bool isRoad;
    bool connects[4];     // 0:North, 1:East, 2:South, 3:West
    Vector3 waypoints[4]; // The target position for the lane in that direction
} RoadTile;

typedef struct GameMap {
    Image image;
    Color *pixels;
    int width;
    int height;
    RoadTile *roads; // 1D array representing the grid
} GameMap;

GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map);
bool IsTileWall(GameMap *map, int gx, int gz);
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

#endif
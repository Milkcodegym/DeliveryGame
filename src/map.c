#include "map.h"

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.image = LoadImage(fileName);
    map.pixels = LoadImageColors(map.image);
    map.width = map.image.width;
    map.height = map.image.height;
    return map;
}

void UnloadGameMap(GameMap *map) {
    UnloadImageColors(map->pixels);
    UnloadImage(map->image);
}

void DrawGameMap(GameMap *map) {
    if (map->width <= 0) return;

    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            Color c = map->pixels[y * map->width + x];
            Vector3 pos = { x * 1.0f, 0.0f, y * 1.0f };

            // Red Buildings
            if (c.r > 200 && c.g < 50 && c.b < 50) {
                DrawCube((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, RED);
                DrawCubeWires((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, MAROON);
            }
            // Dark Roads
            else if (c.a > 0 && c.r < 50 && c.g < 50 && c.b < 50) {
                DrawCube((Vector3){pos.x, 0.05f, pos.z}, 1.0f, 0.1f, 1.0f, DARKGRAY);
            }
        }
    }
}

bool CheckMapCollision(GameMap *map, float x, float z) {
    int gx = (int)(x + 0.5f);
    int gz = (int)(z + 0.5f);
    
    if (gx >= 0 && gx < map->width && gz >= 0 && gz < map->height) {
        Color c = map->pixels[gz * map->width + gx];
        if (c.r > 200 && c.g < 50 && c.b < 50) return true; // Hit Wall
    }
    return false;
}
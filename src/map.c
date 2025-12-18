#include "map.h"
#include "raymath.h"
#include <stdlib.h> // For calloc/free

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.image = LoadImage(fileName);
    map.pixels = LoadImageColors(map.image);
    map.width = map.image.width;
    map.height = map.image.height;

    // Generate Road Network
    if (map.width > 0 && map.height > 0) {
        map.roads = (RoadTile *)calloc(map.width * map.height, sizeof(RoadTile));
        
        // 1. Identify Roads
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                Color c = map.pixels[y * map.width + x];
                // Check for Dark Gray (Roads)
                if (c.a > 0 && c.r < 50 && c.g < 50 && c.b < 50) {
                    map.roads[y * map.width + x].isRoad = true;
                }
            }
        }

        // 2. Build Connections & Waypoints
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                int index = y * map.width + x;
                if (!map.roads[index].isRoad) continue;

                // Check Neighbors (North, East, South, West)
                int dx[] = {0, 1, 0, -1};
                int dy[] = {-1, 0, 1, 0};
                
                // Lane Offsets for Right-Hand Rule
                // N: +X, E: +Z, S: -X, W: -Z
                float offX[] = {0.2f, 0.0f, -0.2f, 0.0f};
                float offZ[] = {0.0f, 0.2f, 0.0f, -0.2f};

                for (int i = 0; i < 4; i++) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];

                    if (nx >= 0 && nx < map.width && ny >= 0 && ny < map.height) {
                        if (map.roads[ny * map.width + nx].isRoad) {
                            map.roads[index].connects[i] = true;
                            map.roads[index].waypoints[i] = (Vector3){ x + offX[i], 0.0f, y + offZ[i] };
                        }
                    }
                }
            }
        }
    }

    return map;
}

void UnloadGameMap(GameMap *map) {
    if (map->roads) free(map->roads);
    UnloadImageColors(map->pixels);
    UnloadImage(map->image);
}

void DrawGameMap(GameMap *map) {
    if (map->width <= 0) return;

    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int index = y * map->width + x;
            Color c = map->pixels[index];
            Vector3 pos = { x * 1.0f, 0.0f, y * 1.0f };

            // Red Buildings
            if (c.r > 200 && c.g < 50 && c.b < 50) {
                DrawCube((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, RED);
                DrawCubeWires((Vector3){pos.x, 1.0f, pos.z}, 1.0f, 2.0f, 1.0f, MAROON);
            }
            // Dark Roads
            else if (c.a > 0 && c.r < 50 && c.g < 50 && c.b < 50) {
                DrawCube((Vector3){pos.x, 0.05f, pos.z}, 1.0f, 0.1f, 1.0f, DARKGRAY);

                // Debug: Draw Traffic Direction Arrows
                if (map->roads) {
                    RoadTile tile = map->roads[index];
                    Vector3 dirs[4] = { {0,0,-1}, {1,0,0}, {0,0,1}, {-1,0,0} }; // N, E, S, W

                    for (int i = 0; i < 4; i++) {
                        if (tile.connects[i]) {
                            Vector3 start = tile.waypoints[i];
                            start.y = 0.15f; // Slightly above road
                            Vector3 end = Vector3Add(start, Vector3Scale(dirs[i], 0.4f));
                            DrawLine3D(start, end, GREEN);
                            DrawCube(end, 0.05f, 0.05f, 0.05f, GREEN); // Arrow head
                        }
                    }
                }
            }
        }
    }
}

// Helper: Checks if a specific grid coordinate (gx, gz) is a red wall
bool IsTileWall(GameMap *map, int gx, int gz) {
    // 1. Check if inside map bounds
    if (gx >= 0 && gx < map->width && gz >= 0 && gz < map->height) {
        // 2. Check color
        Color c = map->pixels[gz * map->width + gx];
        if (c.r > 200 && c.g < 50 && c.b < 50) {
            return true; // It is a wall
        }
    }
    return false; // Not a wall (or out of bounds)
}

bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    // 1. Shift coordinates by +0.5 to align with "Centered" tiles
    // If a wall is at x=5, it visually starts at 4.5.
    // By adding 0.5, we treat 4.5 as 5.0, making the math simpler.
    float checkX = x + 0.5f;
    float checkZ = z + 0.5f;

    // 2. Define the "Hit Box" corners based on the shifted coordinates
    // We shrink the radius slightly (0.9f) to prevent getting stuck on corners
    float padding = radius * 0.9f; 

    // 3. Check the four corners of the player's bounding box
    int minX = (int)floorf(checkX - padding);
    int maxX = (int)floorf(checkX + padding);
    int minZ = (int)floorf(checkZ - padding);
    int maxZ = (int)floorf(checkZ + padding);

    // 4. Check these grid tiles for walls
    // Top-Left
    if (IsTileWall(map, minX, minZ)) return true;
    // Top-Right
    if (IsTileWall(map, maxX, minZ)) return true;
    // Bottom-Left
    if (IsTileWall(map, minX, maxZ)) return true;
    // Bottom-Right
    if (IsTileWall(map, maxX, maxZ)) return true;

    return false;
}
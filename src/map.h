#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- Data Structures ---

typedef struct Node {
    int id;
    Vector2 position;
} Node;

typedef struct Edge {
    int startNode; // Index into nodes array
    int endNode;   // Index into nodes array
    float width;
} Edge;

typedef struct Building {
    Vector2 *footprint; // Array of points
    int pointCount;
    float height;
    Color color;
    Model model;        // The procedurally generated mesh
} Building;

typedef struct GameMap {
    Node *nodes;
    int nodeCount;
    Edge *edges;
    int edgeCount;
    Building *buildings;
    int buildingCount;
} GameMap;

GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map);
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

#endif
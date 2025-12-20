#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- EXISTING CONFIG ---
#define MAX_NODES 50000
#define MAX_EDGES 50000
#define MAX_BUILDINGS 10000
#define MAX_BUILDING_POINTS 100 

// --- NEW CONFIG ---
#define MAX_PATH_NODES 2048
#define MAX_SEARCH_RESULTS 10

// --- STRUCTS ---

typedef struct {
    int id;
    Vector2 position;
} Node;

typedef struct {
    int startNode;
    int endNode;
    float width;
} Edge;

typedef struct {
    float height;
    Color color;
    Vector2 *footprint;
    int pointCount;
    Model model;
} Building;

// --- PATHFINDING STRUCTS ---

typedef struct {
    int targetNodeIndex;
    float distance;
} GraphConnection;

typedef struct {
    GraphConnection* connections;
    int count;
    int capacity;
} NodeGraph;

typedef struct {
    char name[64];
    Vector2 position;
    int iconID; 
} MapLocation;

// <--- FIX IS HERE: Added "GameMap" after "struct"
typedef struct GameMap {
    Node *nodes;
    int nodeCount;
    Edge *edges;
    int edgeCount;
    Building *buildings;
    int buildingCount;

    // Graph Data for Pathfinding
    NodeGraph *graph; 
} GameMap;

// --- FUNCTION PROTOTYPES ---

GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map);
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

// --- NEW PATHFINDING FUNCTIONS ---
void BuildMapGraph(GameMap *map); 
int GetClosestNode(GameMap *map, Vector2 position);
int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen);

// --- SEARCH FUNCTIONS ---
void InitSearchLocations();
int SearchLocations(const char* query, MapLocation* results);

#endif
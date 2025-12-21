#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- CONFIG ---
#define MAX_NODES 50000
#define MAX_EDGES 50000
#define MAX_BUILDINGS 10000
#define MAX_BUILDING_POINTS 100
#define MAX_LOCATIONS 100 
#define MAX_PATH_NODES 2048
#define MAX_SEARCH_RESULTS 10

// --- ENUMS ---
typedef enum {
    LOC_HOUSE = 0,
    LOC_GAS_STATION,
    LOC_SUPERMARKET,
    LOC_SHOP,
    LOC_FOOD_HOT,
    LOC_FOOD_COLD,
    LOC_COFFEE
} LocationType;

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

typedef struct {
    char name[64];
    Vector2 position;
    LocationType type; 
    int iconID; 
} MapLocation;

// --- GRAPH STRUCTS ---
typedef struct {
    int targetNodeIndex;
    float distance;
} GraphConnection;

typedef struct {
    GraphConnection* connections;
    int count;
    int capacity;
} NodeGraph;

typedef struct GameMap {
    Node *nodes;
    int nodeCount;
    Edge *edges;
    int edgeCount;
    Building *buildings;
    int buildingCount;
    
    // Locations Array
    MapLocation *locations;
    int locationCount;

    NodeGraph *graph; 
} GameMap;

// --- PROTOTYPES ---
GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map);

// Updated Collision Signature
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

void BuildMapGraph(GameMap *map); 
int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen);

// Updated Search Signature
int SearchLocations(GameMap *map, const char* query, MapLocation* results);

// New: Visual Effects
void UpdateMapEffects(GameMap *map, Vector3 playerPos);

#endif
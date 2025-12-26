#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- CONFIGURATION ---
#define MAX_NODES 2000
#define MAX_EDGES 2000
#define MAX_BUILDINGS 1000
#define MAX_LOCATIONS 100
#define MAX_AREAS 50
#define MAX_BUILDING_POINTS 20
#define MAX_SEARCH_RESULTS 10
#define MAX_PATH_NODES 2048

// --- DATA STRUCTURES ---

typedef struct {
    int id;
    Vector2 position;
    int flags; 
} Node;

typedef struct {
    int startNode;
    int endNode;
    float width;
    
    // [FIX] Added missing fields for Traffic Logic
    int oneway;   // 1 = True, 0 = False
    int maxSpeed; // Speed limit for this road
} Edge;

typedef struct {
    Vector2 *footprint;
    int pointCount;
    float height;
    Color color;
} Building;

typedef enum {
    LOC_HOUSE = 0,
    LOC_MARKET,
    LOC_FUEL,
    LOC_FOOD,
    LOC_UNKNOWN,
    LOC_CAFE,
    LOC_BAR,
    LOC_SUPERMARKET,
    LOC_RESTAURANT
} LocationType;

typedef struct {
    Vector2 position;
    char name[64];
    LocationType type;
    int iconID;
} MapLocation;

typedef struct {
    Vector2 *points;
    int pointCount;
    int type;
    Color color;
} MapArea;

typedef struct {
    int targetNodeIndex;
    float distance;
} GraphConnection;

typedef struct {
    GraphConnection *connections;
    int count;
    int capacity;
} NodeGraph;

// Tagged struct for forward declaration
typedef struct GameMap {
    Node *nodes;
    int nodeCount;
    
    Edge *edges;
    int edgeCount;
    
    Building *buildings;
    int buildingCount;
    
    MapLocation *locations;
    int locationCount;
    
    MapArea *areas;
    int areaCount;
    
    bool isBatchLoaded;
    NodeGraph *graph;
} GameMap;

// --- FUNCTION PROTOTYPES ---

GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
void DrawGameMap(GameMap *map, Vector3 playerPos);
bool CheckMapCollision(GameMap *map, float x, float z, float radius);
void UpdateMapEffects(GameMap *map, Vector3 playerPos);
void BuildMapGraph(GameMap *map);
int GetClosestNode(GameMap *map, Vector2 position);
int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen);
int SearchLocations(GameMap *map, const char* query, MapLocation* results);

#endif
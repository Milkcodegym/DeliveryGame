#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- CONFIG ---
#define MAX_NODES 50000
#define MAX_EDGES 50000
#define MAX_BUILDINGS 10000
#define MAX_BUILDING_POINTS 100
#define MAX_LOCATIONS 200 
#define MAX_AREAS 1000       // New: Limit for Parks/Water
#define MAX_PATH_NODES 2048
#define MAX_SEARCH_RESULTS 10

// --- ENUMS ---
// Must match the "get_poi_type" mapping in your Python script
typedef enum {
    LOC_NONE = 0,
    LOC_FUEL = 1,
    LOC_FOOD = 2,       // Fast Food
    LOC_CAFE = 3,
    LOC_BAR = 4,
    LOC_MARKET = 5,     // Mini Market
    LOC_SUPERMARKET = 6,
    LOC_RESTAURANT = 7,
    LOC_HOUSE = 8,      // Was Entrance/House in Python
    LOC_PARK = 9,       
    LOC_WATER = 10
} LocationType;

// --- STRUCTS ---

typedef struct {
    int id;
    Vector2 position;
    int flags;          // New: 0=None, 1=Traffic Light, 2=Stop Sign
} Node;

typedef struct {
    int startNode;
    int endNode;
    float width;
    
    // New Traffic Data
    bool oneway;    // true = can only drive Start->End
    int maxSpeed;   // Speed limit in km/h
    int lanes;      // Visual lane count
} Edge;

typedef struct {
    float height;
    Color color;
    Vector2 *footprint;
    int pointCount;
    Model model;        // Note: Check mesh.vertexCount to see if loaded
} Building;

// New: For Parks, Water, etc.
typedef struct {
    int type;           // 0 = Park, 1 = Water
    Color color;
    Vector2 *points;    // Polygon vertices
    int pointCount;
} MapArea;

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
    
    // New: Environment Areas
    MapArea *areas;
    int areaCount;
    
    MapLocation *locations;
    int locationCount;
    Model cityBatch;      // One giant model for all buildings
    bool isBatchLoaded;   // Flag to check if we generated it

    NodeGraph *graph; 
} GameMap;

// --- PROTOTYPES ---
GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);

// UPDATED: Now requires player position for optimization logic
void DrawGameMap(GameMap *map, Vector3 playerPos);

// Collision
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

// Pathfinding
void BuildMapGraph(GameMap *map); 
int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen);

// Search & Logic
int SearchLocations(GameMap *map, const char* query, MapLocation* results);
void UpdateMapEffects(GameMap *map, Vector3 playerPos);
void GenerateMapBatch(GameMap *map);
#endif
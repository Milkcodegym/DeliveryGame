#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- CONSTANTS ---
#define MAX_NODES 200000
#define MAX_EDGES 200000
#define MAX_BUILDINGS 100000
#define MAX_BUILDING_POINTS 30002
#define MAX_LOCATIONS 10000
#define MAX_AREAS 10000
#define MAX_SEARCH_RESULTS 5
#define MAX_EVENTS 5 // Max concurrent events

// --- VISUAL STYLES ---
#define COLOR_ROAD          CLITERAL(Color){ 40, 40, 40, 255 }      // Dark asphalt
#define COLOR_ROAD_MARKING  CLITERAL(Color){ 220, 220, 220, 255 }   // White lines
#define COLOR_PARK          CLITERAL(Color){ 76, 175, 80, 255 }     // Vivid Green
#define COLOR_WATER         CLITERAL(Color){ 33, 150, 243, 255 }    // Blue
#define COLOR_EVENT_PROP    CLITERAL(Color){ 255, 161, 0, 255 }     // Orange for cones/barriers
#define COLOR_EVENT_TEXT    CLITERAL(Color){ 255, 255, 255, 255 }   // White floating text

// --- ENUMS ---
typedef enum {
    LOC_FUEL = 0,
    LOC_FOOD,
    LOC_CAFE,
    LOC_BAR,
    LOC_MARKET,
    LOC_SUPERMARKET,
    LOC_RESTAURANT,
    LOC_HOUSE,
    LOC_MECHANIC, // [NEW] Added Mechanic Location Type
    LOC_COUNT
} LocationType;

// --- DYNAMIC PARK SYSTEM DEFS ---

#define PARK_CHUNK_SIZE 100.0f  // Size of generation chunks (match grid size usually)
#define PARK_RAYS 16            // How many vertices the park polygon has (more = smoother)
#define PARK_MAX_PER_CHUNK 5    // Max parks to generate per chunk

typedef struct {
    Vector2 center;
    Vector2 vertices[PARK_RAYS]; // The outline of the park
    float radius;
    bool active;
} DynamicPark;

typedef struct {
    bool generated;
    int parkCount;
    int parkIndices[PARK_MAX_PER_CHUNK]; // Indices into the global pool
} ParkChunk;

#define MAX_DYNAMIC_PARKS 2048
#define PARK_GRID_ROWS 100
#define PARK_GRID_COLS 100
#define PARK_OFFSET 3000.0f // To handle negative coordinates

typedef struct {
    DynamicPark parks[MAX_DYNAMIC_PARKS];
    int totalParks;
    ParkChunk chunks[PARK_GRID_ROWS][PARK_GRID_COLS];
    bool initialized;
} RuntimeParkSystem;

static RuntimeParkSystem parkSystem = {0};

typedef enum {
    EVENT_NONE = 0,
    EVENT_CRASH,
    EVENT_ROADWORK
} MapEventType;

// --- STRUCTS ---
typedef struct {
    int id;
    Vector2 position;
    int flags; // 0=None, 1=YellowLight, 2=RedLight, 3=StopSign
} Node;

typedef struct {
    int startNode;
    int endNode;
    float width;
    int oneway; // 0=TwoWay, 1=OneWay
    int maxSpeed;
} Edge;

typedef struct {
    float height;
    Color color;
    Vector2 *footprint;
    int pointCount;
    
} Building;

typedef struct {
    char name[64];
    Vector2 position;
    LocationType type;
    int iconID;
} MapLocation;

typedef struct {
    int type; // 0=Park, 1=Water, etc.
    Color color;
    Vector2 *points;
    int pointCount;
} MapArea;

typedef struct {
    int targetNodeIndex;
    float distance;
    int edgeIndex;
} GraphConnection;

typedef struct {
    GraphConnection *connections;
    int count;
    int capacity;
} NodeGraph;

// NEW: Event Struct
typedef struct {
    MapEventType type;
    Vector2 position; // 2D Map Position
    float radius;
    bool active;
    float timer;      // For expiration (fallback)
    char label[64];   // Text to display on the 3D prop
} MapEvent;

// [FIXED] Added struct tag 'GameMap' so forward declarations work
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
    
    NodeGraph *graph; // Navigation Graph
    
    // NEW: Active Events
    MapEvent events[MAX_EVENTS];
    
    bool isBatchLoaded;
} GameMap;

// --- PROTOTYPES ---
GameMap LoadGameMap(const char *fileName);
void UnloadGameMap(GameMap *map);
// UPDATED: Now takes Camera for text labels
void DrawGameMap(GameMap *map, Camera camera); 
void UpdateMapEffects(GameMap *map, Vector3 playerPos);
void DrawZoneMarker(Vector3 pos, Color color);

// Navigation
void BuildMapGraph(GameMap *map);
int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen);
int GetClosestNode(GameMap *map, Vector2 position);

// Search & Collision
int SearchLocations(GameMap *map, const char* query, MapLocation* results);
bool CheckMapCollision(GameMap *map, float x, float z, float radius, bool isCamera);

// NEW: Event System
void TriggerRandomEvent(GameMap *map, Vector3 playerPos, Vector3 playerFwd);
// Trigger specific event (Useful for Dev buttons)
void TriggerSpecificEvent(GameMap *map, MapEventType type, Vector3 playerPos, Vector3 playerFwd);
void ClearEvents(GameMap *map);

// Dev Tools
typedef struct Player Player;
void UpdateDevControls(GameMap *map, Player *player);

// Helpers
void SetMapDestination(GameMap *map, Vector2 dest); // Used by Delivery App
void ResetMapCamera(Vector2 pos); // Required by main.c respawn logic

void UpdateRuntimeParks(GameMap *map, Vector3 playerPos);
void DrawRuntimeParks(Vector3 playerPos);
void UpdateMapStreaming(GameMap *map, Vector3 playerPos);

#endif
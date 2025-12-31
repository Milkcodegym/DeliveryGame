#ifndef MAP_H
#define MAP_H

#include "raylib.h"

// --- CONSTANTS ---
#define MAX_NODES 2000
#define MAX_EDGES 2000
#define MAX_BUILDINGS 1000
#define MAX_BUILDING_POINTS 32
#define MAX_LOCATIONS 100
#define MAX_AREAS 100
#define MAX_SEARCH_RESULTS 5
#define MAX_EVENTS 5 // Max concurrent events

// --- VISUAL STYLES ---
#define COLOR_ROAD          CLITERAL(Color){ 40, 40, 40, 255 }      // Dark asphalt
#define COLOR_ROAD_MARKING  CLITERAL(Color){ 220, 220, 220, 255 }  // White lines
#define COLOR_PARK          CLITERAL(Color){ 76, 175, 80, 255 }    // Vivid Green
#define COLOR_WATER         CLITERAL(Color){ 33, 150, 243, 255 }   // Blue
#define COLOR_EVENT_PROP    CLITERAL(Color){ 255, 161, 0, 255 }    // Orange for cones/barriers
#define COLOR_EVENT_TEXT    CLITERAL(Color){ 255, 255, 255, 255 }  // White floating text

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
    LOC_COUNT
} LocationType;

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
bool CheckMapCollision(GameMap *map, float x, float z, float radius);

// NEW: Event System
void TriggerRandomEvent(GameMap *map, Vector3 playerPos, Vector3 playerFwd);
// Trigger specific event (Useful for Dev buttons)
void TriggerSpecificEvent(GameMap *map, MapEventType type, Vector3 playerPos, Vector3 playerFwd);
void ClearEvents(GameMap *map);

// Dev Tools
void UpdateDevControls(GameMap *map, Vector3 playerPos, Vector3 playerFwd);

// Helpers
void SetMapDestination(GameMap *map, Vector2 dest); // Used by Delivery App

#endif
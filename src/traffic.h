#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "raylib.h"
#include "map.h"

#define MAX_VEHICLES 1

typedef struct Vehicle {
    bool active;
    Vector3 position;
    Vector3 forward;      
    Color color;
    
    // Graph Navigation State
    int currentEdgeIndex; 
    int nextEdgeIndex;    // NEW: We store the upcoming road here
    int startNodeID;      
    int endNodeID;        
    
    float progress;       
    float edgeLength;     
    float speed;          
    
    // Anti-Gridlock
    float stuckTimer;     
} Vehicle;

typedef struct TrafficManager {
    Vehicle vehicles[MAX_VEHICLES];
} TrafficManager;

void InitTraffic(TrafficManager *traffic);
void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt);
void DrawTraffic(TrafficManager *traffic);
Vector3 TrafficCollision(TrafficManager *traffic, float playerPosx, float playerPosz, float player_radius);
int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex);

#endif
#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "raylib.h"
#include "map.h"

#define MAX_VEHICLES 100

// --- SPEED SETTINGS ---
// Adjust these values to slow down/speed up traffic
#define TRAFFIC_BASE_SPEED 5.0f   // Meters per second (approx 18 km/h)
#define TRAFFIC_SPEED_VAR 2.0f    // Random variance (+/- this amount)

typedef struct Vehicle {
    bool active;
    Vector3 position;
    Vector3 forward;  
    Color color;
    
    // Graph Navigation State
    int currentEdgeIndex; 
    int startNodeID;      
    int endNodeID;        
    
    float progress;       
    float edgeLength;     
    float speed;          
} Vehicle;

typedef struct TrafficManager {
    Vehicle vehicles[MAX_VEHICLES];
} TrafficManager;

void InitTraffic(TrafficManager *traffic);
void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt);
void DrawTraffic(TrafficManager *traffic);
bool TrafficCollision(TrafficManager *traffic, float playerPosx, float playerPosz, float player_radius);

#endif
/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 *
 * License: zlib/libpng
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Full license terms: see the LICENSE file.
 * -----------------------------------------------------------------------------
 */

#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "raylib.h"

typedef struct GameMap GameMap;

#define MAX_VEHICLES 150

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
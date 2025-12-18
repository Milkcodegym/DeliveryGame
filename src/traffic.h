#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "raylib.h"
#include "map.h"
#include "player.h"

#define MAX_VEHICLES 50

typedef struct Vehicle {
    bool active;
    Vector3 position;
    Vector3 target;
    float speed;
    int direction; // 0:N, 1:E, 2:S, 3:W
    Color color;
} Vehicle;

typedef struct TrafficManager {
    Vehicle vehicles[MAX_VEHICLES];
    float spawnTimer;
} TrafficManager;

void InitTraffic(TrafficManager *traffic);
void UpdateTraffic(TrafficManager *traffic, Player *player, GameMap *map, float dt);
void DrawTraffic(TrafficManager *traffic);

#endif
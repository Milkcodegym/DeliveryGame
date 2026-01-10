#include "traffic.h"
#include "map.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// --- CONFIGURATION ---
#define SPAWN_RADIUS_MIN 50.0f   
#define SPAWN_RADIUS_MAX 400.0f  
#define DESPAWN_RADIUS 1500.0f   
#define ROAD_HEIGHT 0.5f       

// --- BRAKING CONFIG ---
#define DETECTION_DIST 25.0f   // Distance to start noticing player
#define STOP_DISTANCE 5.0f     // Distance where car wants to be fully stopped
#define ACCEL_RATE 4.0f        
#define BRAKE_RATE 8.0f        // Lowered slightly for smoother stops
#define STUCK_THRESHOLD 5.0f 

extern int GetClosestNode(GameMap *map, Vector2 position);

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
        traffic->vehicles[i].stuckTimer = 0.0f;
    }
}

int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex) {
    if (!map->graph) return -1; 
    if (nodeID >= map->nodeCount) return -1;
    
    NodeGraph *node = &map->graph[nodeID];
    if (node->count == 0) return -1;

    int candidates[8]; 
    int count = 0;

    for (int i = 0; i < node->count; i++) {
        int edgeIdx = node->connections[i].edgeIndex;
        if (edgeIdx == excludeEdgeIndex) continue;
        Edge e = map->edges[edgeIdx];
        if (e.startNode == nodeID) candidates[count++] = edgeIdx;
        else if (e.endNode == nodeID && !e.oneway) candidates[count++] = edgeIdx;
    }
    if (count > 0) return candidates[GetRandomValue(0, count - 1)];

    count = 0;
    for (int i = 0; i < node->count; i++) {
        int edgeIdx = node->connections[i].edgeIndex;
        if (edgeIdx != excludeEdgeIndex) candidates[count++] = edgeIdx;
    }
    if (count > 0) return candidates[GetRandomValue(0, count - 1)];

    if (node->count > 0) return node->connections[0].edgeIndex;
    return -1;
}

float GetDistanceToCarAhead(TrafficManager *traffic, int myIndex, Vector3 myPos, Vector3 myForward, int myEdgeIndex) {
    float closestDist = 9999.0f;
    bool found = false;
    int myNextEdge = traffic->vehicles[myIndex].nextEdgeIndex;

    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (i == myIndex || !traffic->vehicles[i].active) continue;
        int otherEdge = traffic->vehicles[i].currentEdgeIndex;
        if (otherEdge != myEdgeIndex && otherEdge != myNextEdge) continue; 
        
        Vector3 otherPos = traffic->vehicles[i].position;
        Vector3 toOther = Vector3Subtract(otherPos, myPos);
        if (Vector3DotProduct(toOther, myForward) < 0) continue; 
        
        float distSq = Vector3LengthSqr(toOther);
        if (distSq > DETECTION_DIST * DETECTION_DIST) continue; 

        float dist = sqrtf(distSq);
        if (dist < closestDist) { closestDist = dist; found = true; }
    }
    return found ? closestDist : -1.0f;
}

// [NEW] Check distance to player with Field of View (Dot Product)
float GetDistanceToPlayer(Vector3 carPos, Vector3 carFwd, Vector3 playerPos) {
    Vector3 toPlayer = Vector3Subtract(playerPos, carPos);
    float distSq = Vector3LengthSqr(toPlayer);

    // 1. Distance Check
    if (distSq > DETECTION_DIST * DETECTION_DIST) return -1.0f;

    // 2. Direction Check (Dot Product)
    // Normalize vector to player to compare angles
    Vector3 toPlayerNorm = Vector3Normalize(toPlayer);
    float dot = Vector3DotProduct(toPlayerNorm, carFwd);

    // If dot > 0.5, player is roughly within 60 degrees in front.
    // If dot < 0, player is behind the car.
    if (dot < 0.3f) return -1.0f; // Ignore if player is to the side/behind

    return sqrtf(distSq);
}

void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt) {
    if (map->edgeCount == 0 || map->nodeCount == 0 || !map->graph) return;

    static float spawnTimer = 0.0f;
    spawnTimer += dt;

    if (spawnTimer > 0.1f) { 
        spawnTimer = 0.0f;
        int slot = -1;
        for (int i = 0; i < MAX_VEHICLES; i++) { if (!traffic->vehicles[i].active) { slot = i; break; } }

        if (slot != -1) {
            for (int attempt = 0; attempt < 20; attempt++) {
                int randNodeID = GetRandomValue(0, map->nodeCount - 1);
                Vector2 nodeWorld = map->nodes[randNodeID].position;
                float dx = nodeWorld.x - player_position.x;
                float dy = nodeWorld.y - player_position.z;
                float distSq = dx*dx + dy*dy;

                if (distSq > (SPAWN_RADIUS_MIN*SPAWN_RADIUS_MIN) && distSq < (SPAWN_RADIUS_MAX*SPAWN_RADIUS_MAX)) {
                    int edgeIdx = FindNextEdge(map, randNodeID, -1);
                    if (edgeIdx != -1) {
                        // Anti-stacking check
                        Edge e = map->edges[edgeIdx];
                        Vector2 n1 = map->nodes[e.startNode].position;
                        Vector2 n2 = map->nodes[e.endNode].position;
                        Vector3 testPos = { Lerp(n1.x, n2.x, 0.1f), ROAD_HEIGHT, Lerp(n1.y, n2.y, 0.1f) };
                        if (TrafficCollision(traffic, testPos.x, testPos.z, 2.0f).z != -1) continue;

                        Vehicle *v = &traffic->vehicles[slot];
                        v->active = true;
                        v->currentEdgeIndex = edgeIdx;
                        v->speed = 7.5f; 
                        v->stuckTimer = 0.0f;
                        v->startNodeID = e.startNode;
                        v->endNodeID = e.endNode;
                        if (GetRandomValue(0,1)) { v->startNodeID = e.endNode; v->endNodeID = e.startNode; }
                        v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                        v->progress = 0.1f;
                        v->edgeLength = Vector2Distance(n1, n2);
                        v->position = testPos;
                        int c = GetRandomValue(0,4);
                        if(c==0) v->color = RED; else if(c==1) v->color = BLUE; 
                        else if(c==2) v->color = WHITE; else if(c==3) v->color = BLACK;
                        else v->color = DARKGRAY;
                        break; 
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        float dx = v->position.x - player_position.x;
        float dz = v->position.z - player_position.z;
        if ((dx*dx + dz*dz) > DESPAWN_RADIUS * DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        Edge currentEdge = map->edges[v->currentEdgeIndex];
        float maxEdgeSpeed = ((float)currentEdge.maxSpeed) * 0.3f; 
        if(maxEdgeSpeed < 3.0f) maxEdgeSpeed = 3.0f; 
        
        float targetSpeed = maxEdgeSpeed;
        
        // 1. Check Cars Ahead
        float distToCar = GetDistanceToCarAhead(traffic, i, v->position, v->forward, v->currentEdgeIndex);
        if (distToCar != -1.0f) {
            if (distToCar < STOP_DISTANCE) targetSpeed = 0.0f; 
            else {
                float factor = (distToCar - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                targetSpeed = fminf(targetSpeed, maxEdgeSpeed * factor);
            }
        }

        // 2. [NEW] Check Player Ahead (Gradual Braking)
        
        float distToPlayer = GetDistanceToPlayer(v->position, v->forward, player_position);
        
        if (distToPlayer != -1.0f) {
            if (distToPlayer < STOP_DISTANCE) {
                targetSpeed = 0.0f; // Full Stop
            } else {
                // Gradual Slow Down
                // Map distance to a factor 0.0 to 1.0
                float factor = (distToPlayer - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                float playerTarget = maxEdgeSpeed * factor;
                
                // Only apply if it makes us go slower than we already planned
                if (playerTarget < targetSpeed) targetSpeed = playerTarget;
            }
        }
        
        if (v->progress > 0.85f && v->nextEdgeIndex == -1) targetSpeed = 0.0f;

        float rate = (v->speed > targetSpeed) ? BRAKE_RATE : ACCEL_RATE;
        v->speed = Lerp(v->speed, targetSpeed, rate * dt);

        if (v->speed < 0.5f) {
             v->stuckTimer += dt;
             // Don't despawn if waiting for player or another car
             bool waiting = (distToPlayer != -1.0f || distToCar != -1.0f);
             if (!waiting && v->stuckTimer > STUCK_THRESHOLD) v->active = false; 
        } else {
             v->stuckTimer = 0.0f;
        }

        float moveStep = (v->speed * dt) / v->edgeLength;
        v->progress += moveStep;

        if (v->progress >= 1.0f) {
            if (v->nextEdgeIndex != -1) {
                v->currentEdgeIndex = v->nextEdgeIndex;
                v->startNodeID = v->endNodeID;
                Edge nextE = map->edges[v->nextEdgeIndex];
                if (nextE.startNode == v->startNodeID) v->endNodeID = nextE.endNode;
                else v->endNodeID = nextE.startNode;
                v->progress = 0.0f;
                v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                Vector2 n1 = map->nodes[v->startNodeID].position;
                Vector2 n2 = map->nodes[v->endNodeID].position;
                v->edgeLength = Vector2Distance(n1, n2);
            } else {
                v->active = false; 
            }
        }

        Vector2 s2d = map->nodes[v->startNodeID].position;
        Vector2 e2d = map->nodes[v->endNodeID].position;
        Vector3 startPos = { s2d.x, 0, s2d.y };
        Vector3 endPos = { e2d.x, 0, e2d.y };
        Vector3 geoPos = Vector3Lerp(startPos, endPos, v->progress);
        Vector3 dir = Vector3Normalize(Vector3Subtract(endPos, startPos));
        v->forward = dir;
        float roadWidth = (currentEdge.width) * 2.0f; 
        float offsetVal = currentEdge.oneway ? 0.0f : (roadWidth * 0.25f);
        Vector3 right = { -dir.z, 0, dir.x }; 
        v->position = Vector3Add(geoPos, Vector3Scale(right, offsetVal));
        v->position.y = ROAD_HEIGHT; 
    }
}

void DrawTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (v->active) {
            Vector3 size = { 0.9f, 0.7f, 1.9f }; 
            DrawCube(v->position, size.x, size.y, size.z, v->color); 
            DrawCubeWires(v->position, size.x, size.y, size.z, DARKGRAY);
            Vector3 roofPos = Vector3Add(v->position, (Vector3){0, 0.35f, 0}); 
            DrawCube(roofPos, size.x * 0.8f, size.y * 0.5f, size.z * 0.5f, Fade(BLACK, 0.5f));
            Vector3 front = Vector3Add(v->position, Vector3Scale(v->forward, size.z * 0.5f));
            DrawSphere(front, 0.1f, YELLOW); 
        }
    }
}

Vector3 TrafficCollision(TrafficManager *traffic, float playerPosx, float playerPosz, float player_radius) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        float dx = v->position.x - playerPosx;
        float dz = v->position.z - playerPosz;
        float distSq = dx*dx + dz*dz;
        float minDist = 1.0f + player_radius; 
        
        if (distSq < minDist * minDist) {
            float speed = v->speed;
            v->speed = 0.0f;
            return (Vector3){v->forward.x, v->forward.z, speed};
        }
    }
    return (Vector3){0,0,-1};
}
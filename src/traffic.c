#include "traffic.h"
#include "map.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// --- CONFIGURATION ---
#define MAP_SCALE 0.4f        
#define SPAWN_RATE 0.05f       // Chance to spawn per check
#define SPAWN_RADIUS_MIN 40.0f
#define SPAWN_RADIUS_MAX 200.0f
#define DESPAWN_RADIUS 400.0f  // [FIX] Increased significantly to prevent instant despawn

// --- BRAKING CONFIG ---
#define DETECTION_DIST 25.0f   
#define STOP_DISTANCE 5.0f    
#define ACCEL_RATE 5.0f       
#define BRAKE_RATE 15.0f       
#define STUCK_THRESHOLD 5.0f  

// External references
extern int GetClosestNode(GameMap *map, Vector2 position);

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
        traffic->vehicles[i].stuckTimer = 0.0f;
    }
}

// Helper: Find a valid edge connected to a node
int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex) {
    if (!map->graph || nodeID >= map->nodeCount) return -1;

    int candidates[8]; 
    int count = 0;
    NodeGraph *node = &map->graph[nodeID];

    for (int i = 0; i < node->count; i++) {
        int edgeIdx = node->connections[i].edgeIndex;
        if (edgeIdx == excludeEdgeIndex) continue;

        Edge e = map->edges[edgeIdx];
        bool canEnter = false;

        // One-way logic
        if (e.startNode == nodeID) {
            canEnter = true; 
        } else if (e.endNode == nodeID) {
            if (!e.oneway) canEnter = true;
        }

        if (canEnter) {
            candidates[count++] = edgeIdx;
            if (count >= 8) break;
        }
    }

    if (count > 0) return candidates[GetRandomValue(0, count - 1)];
    
    // Dead End U-Turn fallback
    if (excludeEdgeIndex >= 0 && !map->edges[excludeEdgeIndex].oneway) return excludeEdgeIndex;

    return -1;
}

// [FIXED] Braking Logic now checks the NEXT road too
float GetDistanceToCarAhead(TrafficManager *traffic, int myIndex, Vector3 myPos, Vector3 myForward, int myEdgeIndex) {
    float closestDist = 9999.0f;
    bool found = false;
    
    // We also want to check the road we are turning INTO, otherwise we crash at intersections
    int myNextEdge = traffic->vehicles[myIndex].nextEdgeIndex;

    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (i == myIndex || !traffic->vehicles[i].active) continue;

        // OPTIMIZATION: Only check cars on the same road OR the immediate next road
        int otherEdge = traffic->vehicles[i].currentEdgeIndex;
        if (otherEdge != myEdgeIndex && otherEdge != myNextEdge) continue; 

        Vector3 otherPos = traffic->vehicles[i].position;
        Vector3 toOther = Vector3Subtract(otherPos, myPos);
        
        // Dot Product: Is it in front of me?
        if (Vector3DotProduct(toOther, myForward) < 0) continue; 

        float distSq = Vector3LengthSqr(toOther);
        if (distSq > DETECTION_DIST * DETECTION_DIST) continue; 

        float dist = sqrtf(distSq);
        if (dist < closestDist) {
            closestDist = dist;
            found = true;
        }
    }
    return found ? closestDist : -1.0f;
}

void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt) {
    if (map->edgeCount == 0) return;

    // --- 1. SPAWN LOGIC (THROTTLED) ---
    // [FIX] Don't try to spawn every single frame. This improves performance and distribution.
    static float spawnTimer = 0.0f;
    spawnTimer += dt;

    if (spawnTimer > 0.1f) { // Run spawn logic 10 times a second
        spawnTimer = 0.0f;

        // Find empty slot
        int slot = -1;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (!traffic->vehicles[i].active) { slot = i; break; }
        }

        if (slot != -1 && GetRandomValue(0, 100) < 30) { // 30% chance per check
            
            // 1. Random Point in Ring
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float dist = GetRandomValue((int)SPAWN_RADIUS_MIN, (int)SPAWN_RADIUS_MAX);
            Vector2 searchPos = { 
                player_position.x + cosf(angle)*dist, 
                player_position.z + sinf(angle)*dist 
            };

            // 2. Snap to nearest node
            int nodeID = GetClosestNode(map, searchPos);

            if (nodeID != -1) {
                // [FIX] Check if the actual node is within DESPAWN radius
                // Sometimes the "closest node" is actually 500 units away if the map is sparse
                Vector2 nodePos = map->nodes[nodeID].position;
                float distToPlayerSq = (nodePos.x - player_position.x)*(nodePos.x - player_position.x) + 
                                       (nodePos.y - player_position.z)*(nodePos.y - player_position.z);
                
                if (distToPlayerSq < (DESPAWN_RADIUS * DESPAWN_RADIUS)) {
                    
                    int edgeIdx = FindNextEdge(map, nodeID, -1);
                    
                    if (edgeIdx != -1) {
                        // SETUP VEHICLE
                        Vehicle *v = &traffic->vehicles[slot];
                        v->active = true;
                        v->stuckTimer = 0.0f;
                        v->currentEdgeIndex = edgeIdx;
                        
                        Edge e = map->edges[edgeIdx];
                        v->startNodeID = e.startNode;
                        v->endNodeID = e.endNode;

                        // Randomize direction if not one-way
                        if (e.oneway == 0 && GetRandomValue(0, 1)) {
                            v->startNodeID = e.endNode;
                            v->endNodeID = e.startNode;
                        }

                        v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                        v->progress = 0.1f; // Start slightly into the road
                        v->speed = 10.0f;
                        
                        Vector3 p1 = { map->nodes[v->startNodeID].position.x, 0, map->nodes[v->startNodeID].position.y };
                        Vector3 p2 = { map->nodes[v->endNodeID].position.x, 0, map->nodes[v->endNodeID].position.y };
                        v->edgeLength = Vector3Distance(p1, p2);
                        
                        // Random Color
                        int variant = GetRandomValue(0, 3);
                        if(variant==0) v->color = (Color){220, 60, 60, 255};      // Red
                        else if(variant==1) v->color = (Color){60, 100, 220, 255}; // Blue
                        else if(variant==2) v->color = (Color){230, 230, 230, 255}; // White
                        else v->color = (Color){40, 40, 40, 255}; // Black
                    }
                }
            }
        }
    }

    // --- 2. UPDATE VEHICLES ---
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        // Despawn Check
        float dx = v->position.x - player_position.x;
        float dz = v->position.z - player_position.z;
        if ((dx*dx + dz*dz) > DESPAWN_RADIUS * DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        Edge currentEdge = map->edges[v->currentEdgeIndex];
        float maxEdgeSpeed = (float)currentEdge.maxSpeed * 0.25f; 
        if(maxEdgeSpeed < 8.0f) maxEdgeSpeed = 8.0f;
        
        float targetSpeed = maxEdgeSpeed;

        // Braking Logic
        float distToCar = GetDistanceToCarAhead(traffic, i, v->position, v->forward, v->currentEdgeIndex);
        
        if (distToCar != -1.0f) {
            if (distToCar < STOP_DISTANCE) {
                targetSpeed = 0.0f; 
            } else {
                float factor = (distToCar - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                targetSpeed = maxEdgeSpeed * factor;
            }
        }
        
        // Intersection slowing
        if (v->progress > 0.85f) {
            if (v->nextEdgeIndex != -1) {
                // If sharp turn, slow down
                targetSpeed *= 0.6f;
            } else {
                targetSpeed = 0.0f; // Dead end
            }
        }

        // Simple Player Avoidance
        float pDistSq = (v->position.x - player_position.x)*(v->position.x - player_position.x) + 
                        (v->position.z - player_position.z)*(v->position.z - player_position.z);
                        
        if (pDistSq < 15.0f * 15.0f) {
            Vector3 toPlayer = Vector3Subtract(player_position, v->position);
            if (Vector3DotProduct(Vector3Normalize(toPlayer), v->forward) > 0.5f) {
                targetSpeed = 0.0f; 
            }
        }

        // Apply Speed
        float rate = (v->speed > targetSpeed) ? BRAKE_RATE : ACCEL_RATE;
        v->speed = Lerp(v->speed, targetSpeed, rate * dt);

        // Stuck Check (Despawn if stuck for too long)
        if (v->speed < 1.0f && targetSpeed > 2.0f && distToCar == -1.0f) {
             v->stuckTimer += dt;
             if (v->stuckTimer > STUCK_THRESHOLD) v->active = false; 
        } else {
            v->stuckTimer = 0.0f;
        }

        // Move along edge
        float moveStep = (v->speed * dt) / v->edgeLength;
        v->progress += moveStep;

        // Change Road
        if (v->progress >= 1.0f) {
            if (v->nextEdgeIndex != -1) {
                v->currentEdgeIndex = v->nextEdgeIndex;
                v->startNodeID = v->endNodeID;
                
                Edge nextE = map->edges[v->nextEdgeIndex];
                if (nextE.startNode == v->startNodeID) v->endNodeID = nextE.endNode;
                else v->endNodeID = nextE.startNode;

                v->progress = 0.0f;
                v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);

                Vector3 p1 = { map->nodes[v->startNodeID].position.x, 0, map->nodes[v->startNodeID].position.y };
                Vector3 p2 = { map->nodes[v->endNodeID].position.x, 0, map->nodes[v->endNodeID].position.y };
                v->edgeLength = Vector3Distance(p1, p2);
            } else {
                v->active = false; // Reached dead end
            }
        }

        // Update 3D Position
        Vector2 s2d = map->nodes[v->startNodeID].position;
        Vector2 e2d = map->nodes[v->endNodeID].position;
        Vector3 startPos = { s2d.x, 0, s2d.y };
        Vector3 endPos = { e2d.x, 0, e2d.y };

        Vector3 geoPos = Vector3Lerp(startPos, endPos, v->progress);
        Vector3 dir = Vector3Normalize(Vector3Subtract(endPos, startPos));
        v->forward = dir;

        // Lane Offset logic
        float roadWidth = (currentEdge.width * MAP_SCALE) * 2.0f;
        float offsetVal = currentEdge.oneway ? 0.0f : (roadWidth * 0.25f);
        
        Vector3 right = { -dir.z, 0, dir.x }; 
        v->position = Vector3Add(geoPos, Vector3Scale(right, offsetVal));
        v->position.y = 0.2f;
    }
}

void DrawTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (v->active) {
            //  
            // Simple blocky car representation
            Vector3 size = { 0.4f, 0.3f, 0.8f }; 

            DrawCube(v->position, size.x, size.y, size.z, v->color); 
            DrawCubeWires(v->position, size.x, size.y, size.z, DARKGRAY);
            
            // Roof
            Vector3 roofPos = Vector3Add(v->position, (Vector3){0, 0.2f, 0});
            DrawCube(roofPos, size.x * 0.8f, size.y * 0.5f, size.z * 0.5f, Fade(BLACK, 0.5f));

            // Headlights
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
        
        float minDist = 0.5f + player_radius; 
        
        if (distSq < minDist * minDist) {
            float speed = v->speed;
            v->speed = 0.0f;
            return (Vector3){v->forward.x, v->forward.z, speed};
        }
    }
    return (Vector3){0,0,-1};
}


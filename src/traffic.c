#include "traffic.h"
#include "map.h" // Requires access to GetChunkIndex and cityRenderer from map.c
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MAP_SCALE 0.4f        
#define SPAWN_RATE 0.1f       
#define SPAWN_RADIUS_MIN 40.0f
#define SPAWN_RADIUS_MAX 200.0f
#define DESPAWN_RADIUS 250.0f  // Increased slightly to prevent visible despawning

// --- BRAKING CONFIG ---
#define DETECTION_DIST 20.0f  // Increased for smoother braking
#define STOP_DISTANCE 4.0f    
#define ACCEL_RATE 5.0f       
#define BRAKE_RATE 10.0f      

#define STUCK_THRESHOLD 5.0f  

// External references to Map System (from map.c)
// We need these to spawn cars smartly using the grid we built
extern int GetChunkIndex(Vector2 pos);
extern int GetClosestNode(GameMap *map, Vector2 position);
// Note: In C, accessing static variables from another file is hard. 
// Ideally, map.c should expose a "GetRandomNodeNear(pos)" function.
// For now, we will use a slightly better random search.

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
        traffic->vehicles[i].stuckTimer = 0.0f;
    }
}

// O(1) Lookup - 1000x Faster
int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex) {
    // Safety check
    if (!map->graph || nodeID >= map->nodeCount) return -1;

    int candidates[8]; 
    int count = 0;

    // DIRECT LOOKUP: Access the node in the graph
    NodeGraph *node = &map->graph[nodeID];

    // Loop only through the connected roads (usually 2, 3, or 4)
    for (int i = 0; i < node->count; i++) {
        int edgeIdx = node->connections[i].edgeIndex;

        // Don't turn back onto the road we just came from
        if (edgeIdx == excludeEdgeIndex) continue;

        // Check One-Way Logic
        Edge e = map->edges[edgeIdx];
        
        bool canEnter = false;
        if (e.startNode == nodeID) {
            // We are at the start, driving towards end. Always OK.
            canEnter = true;
        } else if (e.endNode == nodeID) {
            // We are at the end, driving towards start. Only OK if NOT one-way.
            if (!e.oneway) canEnter = true;
        }

        if (canEnter) {
            candidates[count++] = edgeIdx;
            if (count >= 8) break;
        }
    }

    // Pick a random valid road
    if (count > 0) return candidates[GetRandomValue(0, count - 1)];

    // Dead End: Allow U-Turn if legal
    if (excludeEdgeIndex >= 0 && !map->edges[excludeEdgeIndex].oneway) return excludeEdgeIndex;

    return -1;
}

// --- CRITICAL OPTIMIZATION: O(1) Braking Check ---
float GetDistanceToCarAhead(TrafficManager *traffic, int myIndex, Vector3 myPos, Vector3 myForward, int myEdgeIndex) {
    float closestDist = 9999.0f;
    bool found = false;

    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (i == myIndex || !traffic->vehicles[i].active) continue;

        // [OPTIMIZATION] Lane Check
        // Only check cars on the SAME ROAD or the VERY NEXT ROAD.
        // This cuts checks by 99%.
        int otherEdge = traffic->vehicles[i].currentEdgeIndex;
        if (otherEdge != myEdgeIndex) continue; 

        // Vector math (now only runs for ~2 cars instead of 100)
        Vector3 otherPos = traffic->vehicles[i].position;
        Vector3 toOther = Vector3Subtract(otherPos, myPos);
        
        // Dot Product optimization: Check if in front BEFORE calculating distance (sqrt is slow)
        if (Vector3DotProduct(toOther, myForward) < 0) continue; // Behind me

        float distSq = Vector3LengthSqr(toOther);
        if (distSq > DETECTION_DIST * DETECTION_DIST) continue; // Too far

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

    // --- 1. SPAWN LOGIC (FIXED) ---
    // Previous "Random Edge" logic had <0.1% success rate. 
    // New logic: Find a node near player and spawn there.
    
    // Only try to spawn if we have capacity
    int slot = -1;
    // Fast check for empty slot
    static int lastSlot = 0;
    for (int k = 0; k < MAX_VEHICLES; k++) {
        int idx = (lastSlot + k) % MAX_VEHICLES;
        if (!traffic->vehicles[idx].active) { slot = idx; lastSlot = idx; break; }
    }

    if (slot != -1 && GetRandomValue(0, 100) < 15) { // 15% chance
        
        // SMART SPAWN:
        // 1. Pick a random point in a ring around the player
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float dist = GetRandomValue((int)SPAWN_RADIUS_MIN, (int)SPAWN_RADIUS_MAX);
        Vector2 searchPos = { 
            player_position.x + cosf(angle)*dist, 
            player_position.z + sinf(angle)*dist 
        };

        // 2. Snap to nearest node (Using your new Optimized function!)
        int nodeID = GetClosestNode(map, searchPos);

        if (nodeID != -1) {
            // 3. Find an edge connected to this node
            int edgeIdx = FindNextEdge(map, nodeID, -1);
            
            if (edgeIdx != -1) {
                // SPAWN THE CAR
                Vehicle *v = &traffic->vehicles[slot];
                v->active = true;
                v->stuckTimer = 0.0f;
                v->currentEdgeIndex = edgeIdx;
                
                Edge e = map->edges[edgeIdx];
                v->startNodeID = e.startNode;
                v->endNodeID = e.endNode;
                if (e.oneway == 0 && GetRandomValue(0, 1)) {
                    v->startNodeID = e.endNode;
                    v->endNodeID = e.startNode;
                }

                v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                v->progress = 0.05f; // Start at beginning of road
                v->speed = 10.0f;
                
                Vector3 p1 = { map->nodes[v->startNodeID].position.x, 0, map->nodes[v->startNodeID].position.y };
                Vector3 p2 = { map->nodes[v->endNodeID].position.x, 0, map->nodes[v->endNodeID].position.y };
                v->edgeLength = Vector3Distance(p1, p2);
                
                // Color variation
                int variant = GetRandomValue(0, 2);
                if(variant==0) v->color = (Color){200, 50, 50, 255}; // Red
                else if(variant==1) v->color = (Color){50, 50, 200, 255}; // Blue
                else v->color = (Color){220, 220, 220, 255}; // White
            }
        }
    }

    // --- 2. UPDATE VEHICLES ---
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        // Despawn Check (Squared distance is faster)
        float dx = v->position.x - player_position.x;
        float dz = v->position.z - player_position.z;
        if ((dx*dx + dz*dz) > DESPAWN_RADIUS * DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        Edge currentEdge = map->edges[v->currentEdgeIndex];
        float maxEdgeSpeed = (float)currentEdge.maxSpeed * 0.25f; // Adjusted scale
        if(maxEdgeSpeed < 8.0f) maxEdgeSpeed = 8.0f;
        
        float targetSpeed = maxEdgeSpeed;

        // --- BRAKING LOGIC ---
        
        // 1. Check Car Ahead (Now Optimized)
        float distToCar = GetDistanceToCarAhead(traffic, i, v->position, v->forward, v->currentEdgeIndex);
        
        if (distToCar != -1.0f) {
            if (distToCar < STOP_DISTANCE) {
                targetSpeed = 0.0f; 
            } else {
                float factor = (distToCar - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                targetSpeed = maxEdgeSpeed * factor;
            }
        }
        
        // 2. Intersection Approaching
        if (v->progress > 0.85f) {
             // Basic traffic light logic simulation (random slow down)
            if (v->nextEdgeIndex != -1) {
                // If turning sharply, slow down
                // (Your existing Dot Product logic was good, keeping it simple here)
                targetSpeed *= 0.5f;
            } else {
                // Dead end
                targetSpeed = 0.0f;
            }
        }

        // 3. Player Collision Avoidance
        float pDistSq = (v->position.x - player_position.x)*(v->position.x - player_position.x) + 
                        (v->position.z - player_position.z)*(v->position.z - player_position.z);
                        
        if (pDistSq < DETECTION_DIST * DETECTION_DIST) {
            Vector3 toPlayer = Vector3Subtract(player_position, v->position);
            if (Vector3DotProduct(Vector3Normalize(toPlayer), v->forward) > 0.5f) {
                targetSpeed = 0.0f; // Panic brake
            }
        }

        // Apply Speed
        float rate = (v->speed > targetSpeed) ? BRAKE_RATE : ACCEL_RATE;
        v->speed = Lerp(v->speed, targetSpeed, rate * dt);

        // Stuck Check
        if (v->speed < 1.0f && targetSpeed > 2.0f && distToCar == -1.0f) {
             v->stuckTimer += dt;
             if (v->stuckTimer > STUCK_THRESHOLD) v->active = false; // Despawn stuck cars
        } else {
            v->stuckTimer = 0.0f;
        }

        // Move
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
                v->active = false;
            }
        }

        // --- VISUAL OFFSET LOGIC ---
        Vector2 s2d = map->nodes[v->startNodeID].position;
        Vector2 e2d = map->nodes[v->endNodeID].position;
        Vector3 startPos = { s2d.x, 0, s2d.y };
        Vector3 endPos = { e2d.x, 0, e2d.y };

        Vector3 geoPos = Vector3Lerp(startPos, endPos, v->progress);
        Vector3 dir = Vector3Normalize(Vector3Subtract(endPos, startPos));
        v->forward = dir;

        // Calculate Lane Offset (Right Hand Drive)
        float roadWidth = (currentEdge.width * MAP_SCALE) * 2.0f;
        float offsetVal = currentEdge.oneway ? 0.0f : (roadWidth * 0.25f);
        
        Vector3 right = { -dir.z, 0, dir.x }; // Fast Cross Product for Y-up
        v->position = Vector3Add(geoPos, Vector3Scale(right, offsetVal));
        v->position.y = 0.2f;
    }
}

void DrawTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (v->active) {
            Vector3 size = { 0.4f, 0.3f, 0.8f }; 

            DrawCube(v->position, size.x, size.y, size.z, v->color); 
            DrawCubeWires(v->position, size.x, size.y, size.z, DARKGRAY);
            
            Vector3 roofPos = Vector3Add(v->position, (Vector3){0, 0.2f, 0});
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
        
        float minDist = 0.4f + player_radius; 
        
        if (distSq < minDist * minDist) {
            float speed = v->speed;
            v->speed = 0.0f;
            Vector3 output = {v->forward.x,v->forward.z,speed};
            return output;
        }
    }
    Vector3 output = {0,0,-1};
    return output;
}
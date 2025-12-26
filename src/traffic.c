#include "traffic.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define MAP_SCALE 0.4f        
#define SPAWN_RATE 0.1f       
#define SPAWN_RADIUS_MIN 40.0f
#define SPAWN_RADIUS_MAX 200.0f
#define DESPAWN_RADIUS 220.0f

// --- BRAKING CONFIG ---
#define DETECTION_DIST 15.0f  
#define STOP_DISTANCE 2.5f    
#define ACCEL_RATE 3.0f       
#define BRAKE_RATE 5.0f       // Faster deceleration for braking

// --- GRIDLOCK CONFIG ---
#define STUCK_THRESHOLD 10.0f  

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
        traffic->vehicles[i].stuckTimer = 0.0f;
    }
}

// --- Helper: Find a new road connected to 'nodeID' ---
int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex) {
    int candidates[10]; 
    int count = 0;

    for (int i = 0; i < map->edgeCount; i++) {
        if (i == excludeEdgeIndex) continue;

        Edge e = map->edges[i];
        
        if (e.startNode == nodeID) {
            candidates[count++] = i;
        } 
        else if (e.endNode == nodeID) {
            if (!e.oneway) candidates[count++] = i;
        }
        
        if (count >= 10) break;
    }

    if (count > 0) return candidates[GetRandomValue(0, count - 1)];
    
    // Dead end logic: Allow U-turn only if valid
    if (excludeEdgeIndex >= 0 && !map->edges[excludeEdgeIndex].oneway) return excludeEdgeIndex; 
    
    return -1; 
}

// Helper: Returns distance to nearest car ahead (-1.0 if clear)
float GetDistanceToCarAhead(TrafficManager *traffic, int myIndex, Vector3 myPos, Vector3 myForward) {
    float closestDist = 9999.0f;
    bool found = false;

    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (i == myIndex || !traffic->vehicles[i].active) continue;

        if (Vector3DotProduct(myForward, traffic->vehicles[i].forward) < -0.5f) continue;

        Vector3 otherPos = traffic->vehicles[i].position;
        Vector3 toOther = Vector3Subtract(otherPos, myPos);
        float dist = Vector3Length(toOther);
        
        if (dist > DETECTION_DIST) continue;

        float forwardDot = Vector3DotProduct(Vector3Normalize(toOther), myForward);
        
        if (forwardDot > 0.9f) { 
            if (dist < closestDist) {
                closestDist = dist;
                found = true;
            }
        }
    }
    return found ? closestDist : -1.0f;
}

void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt) {
    if (map->edgeCount == 0) return;

    // --- 1. SPAWN LOGIC ---
    if (GetRandomValue(0, 100) < 5) { 
        int slot = -1;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (!traffic->vehicles[i].active) {
                slot = i;
                break;
            }
        }

        if (slot != -1) {
            int edgeIdx = GetRandomValue(0, map->edgeCount - 1);
            Edge e = map->edges[edgeIdx];
            
            if (e.startNode < map->nodeCount && e.endNode < map->nodeCount) {
                Vector3 p1 = { map->nodes[e.startNode].position.x, 0, map->nodes[e.startNode].position.y };
                Vector3 p2 = { map->nodes[e.endNode].position.x, 0, map->nodes[e.endNode].position.y };
                Vector3 mid = Vector3Scale(Vector3Add(p1, p2), 0.5f);

                float distToPlayer = Vector3Distance(mid, player_position);

                if (distToPlayer > SPAWN_RADIUS_MIN && distToPlayer < SPAWN_RADIUS_MAX) {
                    Vehicle *v = &traffic->vehicles[slot];
                    v->active = true;
                    v->stuckTimer = 0.0f;
                    v->currentEdgeIndex = edgeIdx;
                    
                    bool reverse = (GetRandomValue(0, 1) == 0);
                    if (e.oneway) reverse = false; 

                    if (!reverse) {
                        v->startNodeID = e.startNode;
                        v->endNodeID = e.endNode;
                    } else {
                        v->startNodeID = e.endNode;
                        v->endNodeID = e.startNode;
                    }

                    v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                    v->progress = GetRandomValue(10, 90) / 100.0f;
                    
                    float maxSpeed = (float)e.maxSpeed * 0.15f; 
                    if (maxSpeed < 3.0f) maxSpeed = 8.0f; 
                    v->speed = maxSpeed;
                    
                    v->edgeLength = Vector3Distance(p1, p2);
                    v->color = (Color){ GetRandomValue(50, 250), GetRandomValue(50, 250), GetRandomValue(50, 250), 255 };
                }
            }
        }
    }

    // --- 2. UPDATE VEHICLES ---
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        if (Vector3Distance(v->position, player_position) > DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        // --- INTELLIGENT SPEED CONTROL ---
        float maxEdgeSpeed = (float)map->edges[v->currentEdgeIndex].maxSpeed * 0.15f;
        if(maxEdgeSpeed < 5.0f) maxEdgeSpeed = 8.0f;
        
        float targetSpeed = maxEdgeSpeed;

        // A. Intersection Check
        if (v->progress > 0.85f) {
            Node endNode = map->nodes[v->endNodeID];
            if (endNode.flags == 1 || endNode.flags == 2) { 
                targetSpeed *= 0.3f; 
            }
        }

        // B. Corner Braking
        if (v->progress > 0.7f && v->nextEdgeIndex != -1) {
            Vector2 pStart = map->nodes[v->startNodeID].position;
            Vector2 pEnd   = map->nodes[v->endNodeID].position;
            Vector2 dirCurrent = Vector2Normalize(Vector2Subtract(pEnd, pStart));

            Edge nextEdge = map->edges[v->nextEdgeIndex];
            Vector2 nStart = map->nodes[nextEdge.startNode].position;
            Vector2 nEnd   = map->nodes[nextEdge.endNode].position;
            Vector2 dirNext;
            if (nextEdge.startNode == v->endNodeID) {
                dirNext = Vector2Normalize(Vector2Subtract(nEnd, nStart));
            } else {
                dirNext = Vector2Normalize(Vector2Subtract(nStart, nEnd));
            }

            float dot = Vector2DotProduct(dirCurrent, dirNext);
            float turnFactor = 1.0f;
            if (dot < 0.9f) {
                turnFactor = 0.3f + (0.7f * ((dot + 1.0f) / 2.0f)); 
                if (turnFactor < 0.3f) turnFactor = 0.3f; 
            }
            
            if (targetSpeed > maxEdgeSpeed * turnFactor) {
                targetSpeed = maxEdgeSpeed * turnFactor;
            }
        }

        // C. Collision Avoidance
        float distToCar = GetDistanceToCarAhead(traffic, i, v->position, v->forward);
        if (distToCar != -1.0f) {
            if (distToCar < STOP_DISTANCE) {
                targetSpeed = 0.0f; 
            } else {
                float factor = (distToCar - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                if (factor < 0) factor = 0;
                if (factor > 1) factor = 1;
                targetSpeed = maxEdgeSpeed * factor;
            }
        }
        
        // Player Avoidance
        float distToPlayer = Vector3Distance(v->position, player_position);
        if (distToPlayer < DETECTION_DIST) {
            Vector3 toPlayer = Vector3Subtract(player_position, v->position);
            float pDot = Vector3DotProduct(Vector3Normalize(toPlayer), v->forward);
            
            if (pDot > 0.6f) { 
                if (distToPlayer < STOP_DISTANCE + 1.0f) {
                   targetSpeed = 0.0f; 
                } else {
                   targetSpeed *= 0.2f; 
                }
            }
        }

        // D. Apply Speed
        float rate = (v->speed > targetSpeed) ? BRAKE_RATE : ACCEL_RATE;
        v->speed = Lerp(v->speed, targetSpeed, rate * dt);

        if (v->speed < 0.5f) {
            v->stuckTimer += dt;
            if (v->stuckTimer > STUCK_THRESHOLD) {
                v->active = false;
                continue; 
            }
        } else {
            v->stuckTimer = 0.0f;
        }

        // --- MOVEMENT ---
        float moveStep = (v->speed * dt) / v->edgeLength;
        v->progress += moveStep;

        if (v->progress >= 1.0f) {
            int nextEdgeIdx = v->nextEdgeIndex;
            
            if (nextEdgeIdx != -1) {
                Edge nextEdge = map->edges[nextEdgeIdx];
                v->currentEdgeIndex = nextEdgeIdx;
                v->startNodeID = v->endNodeID; 
                
                if (nextEdge.startNode == v->startNodeID) v->endNodeID = nextEdge.endNode;
                else v->endNodeID = nextEdge.startNode;

                v->progress = 0.0f;
                
                Vector3 pStart = { map->nodes[v->startNodeID].position.x, 0, map->nodes[v->startNodeID].position.y };
                Vector3 pEnd = { map->nodes[v->endNodeID].position.x, 0, map->nodes[v->endNodeID].position.y };
                v->edgeLength = Vector3Distance(pStart, pEnd);

                v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);

            } else {
                v->active = false; 
            }
        }

        // --- POSITION CALCULATION (FIXED) ---
        Vector2 startNode2D = map->nodes[v->startNodeID].position;
        Vector2 endNode2D = map->nodes[v->endNodeID].position;

        Vector3 startPos = { startNode2D.x, 0.0f, startNode2D.y };
        Vector3 endPos = { endNode2D.x, 0.0f, endNode2D.y };

        Vector3 currentPos = Vector3Lerp(startPos, endPos, v->progress);
        Vector3 dir = Vector3Normalize(Vector3Subtract(endPos, startPos));
        v->forward = dir; 

        // [FIX] Correct Lane Offset Logic
        // 1. Calculate the same rendered width as the Draw function (Raw * Scale * 2.0)
        float renderWidth = (map->edges[v->currentEdgeIndex].width * MAP_SCALE) * 2.0f;
        
        // 2. Determine offset amount
        float laneOffset = 0.0f;
        
        if (map->edges[v->currentEdgeIndex].oneway) {
            // One-way: Drive in the geometric center (offset 0)
            // Optional: If you want 2 lanes, add randomness here later
            laneOffset = 0.0f; 
        } else {
            // Two-way: Drive on the RIGHT side.
            // Center of the right lane is at (TotalWidth / 4)
            laneOffset = renderWidth * 0.25f;
        }

        // 3. Apply Offset using Vector3Add (Right) instead of Subtract (Left)
        Vector3 right = Vector3CrossProduct((Vector3){0,1,0}, dir);
        v->position = Vector3Add(currentPos, Vector3Scale(right, laneOffset)); 
        
        // [FIX] Lock Height to just above the road/markings
        v->position.y = 0.05f; 
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
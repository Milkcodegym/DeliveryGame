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

#include "traffic.h"
#include "raymath.h"
#include "rlgl.h"
#include "map.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// --- CONFIGURATION ---
#define SPAWN_RADIUS_MIN 100.0f   
#define SPAWN_RADIUS_MAX 200.0f  
#define DESPAWN_RADIUS 300.0f   
#define ROAD_HEIGHT 0.5f       

// --- BRAKING & SPEED CONFIG ---
#define DETECTION_DIST 15.0f   
#define STOP_DISTANCE 4.0f     
#define ACCEL_RATE 3.0f        
#define BRAKE_RATE 12.0f       
#define STUCK_THRESHOLD 5.0f 

extern int GetClosestNode(GameMap *map, Vector2 position);

/*
 * Description: Initializes the traffic manager, deactivating all vehicle slots.
 * Parameters:
 * - traffic: Pointer to the TrafficManager struct.
 * Returns: None.
 */
void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
        traffic->vehicles[i].stuckTimer = 0.0f;
    }
}

/*
 * Description: Finds the next valid edge connected to a node, avoiding U-turns if possible.
 * Parameters:
 * - map: Pointer to the GameMap.
 * - nodeID: The current node index.
 * - excludeEdgeIndex: The edge index the vehicle just came from.
 * Returns: The index of the next edge, or -1 if none found.
 */
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

/*
 * Description: Checks for vehicles directly ahead on the same road segment to prevent collisions.
 * Parameters:
 * - traffic: Pointer to TrafficManager.
 * - myIndex: The vehicle's own index (to ignore self).
 * - myPos: Current position.
 * - myForward: Forward vector.
 * - myEdgeIndex: Current road edge index.
 * Returns: Distance to the nearest car ahead, or -1.0f if clear.
 */
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
        if (Vector3DotProduct(myForward, traffic->vehicles[i].forward) < -0.5f) continue;

        float distSq = Vector3LengthSqr(toOther);
        if (distSq > DETECTION_DIST * DETECTION_DIST) continue; 

        float dist = sqrtf(distSq);
        if (dist < closestDist) { closestDist = dist; found = true; }
    }
    return found ? closestDist : -1.0f;
}

/*
 * Description: Calculates the distance to the player if they are directly in the vehicle's path.
 * Parameters:
 * - carPos: Vehicle position.
 * - carFwd: Vehicle forward vector.
 * - playerPos: Player's position.
 * Returns: Distance to player, or -1.0f if not in path.
 */
float GetDistanceToPlayer(Vector3 carPos, Vector3 carFwd, Vector3 playerPos) {
    Vector3 toPlayer = Vector3Subtract(playerPos, carPos);
    float distSq = Vector3LengthSqr(toPlayer);

    if (distSq > DETECTION_DIST * DETECTION_DIST) return -1.0f;

    Vector3 toPlayerNorm = Vector3Normalize(toPlayer);
    float dot = Vector3DotProduct(toPlayerNorm, carFwd);

    if (dot < 0.2f) return -1.0f; 

    return sqrtf(distSq);
}

/*
 * Description: Updates logic for all traffic vehicles: spawning, movement, pathfinding, and obstacle avoidance.
 * Parameters:
 * - traffic: Pointer to TrafficManager.
 * - player_position: Player's position (for spawn/despawn logic).
 * - map: Pointer to GameMap.
 * - dt: Delta Time.
 * Returns: None.
 */
void UpdateTraffic(TrafficManager *traffic, Vector3 player_position, GameMap *map, float dt) {
    if (map->edgeCount == 0 || map->nodeCount == 0 || !map->graph) return;

    // --- 1. SPAWNING LOGIC ---
    static float spawnTimer = 0.0f;
    spawnTimer += dt;
    if (spawnTimer > 0.5f) {
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
                        Edge e = map->edges[edgeIdx];
                        Vector2 n1 = map->nodes[e.startNode].position;
                        Vector2 n2 = map->nodes[e.endNode].position;
                        Vector3 testPos = { Lerp(n1.x, n2.x, 0.1f), ROAD_HEIGHT, Lerp(n1.y, n2.y, 0.1f) };
                        if (TrafficCollision(traffic, testPos.x, testPos.z, 2.0f).z != -1) continue;

                        Vehicle *v = &traffic->vehicles[slot];
                        v->active = true;
                        v->currentEdgeIndex = edgeIdx;
                        v->speed = 0.0f; 
                        v->stuckTimer = 0.0f;
                        v->startNodeID = e.startNode;
                        v->endNodeID = e.endNode;
                        if (GetRandomValue(0,1)) { v->startNodeID = e.endNode; v->endNodeID = e.startNode; }
                        v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
                        v->progress = 0.1f;
                        v->edgeLength = Vector2Distance(n1, n2);
                        v->position = testPos;
                        v->color = (Color){ GetRandomValue(80, 200), GetRandomValue(80, 200), GetRandomValue(80, 200), 255 };
                        break; 
                    }
                }
            }
        }
    }

    // --- 2. UPDATE INDIVIDUAL VEHICLES ---
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        // Despawn if too far
        float dx_p = v->position.x - player_position.x;
        float dz_p = v->position.z - player_position.z;
        if ((dx_p*dx_p + dz_p*dz_p) > DESPAWN_RADIUS * DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        Edge currentEdge = map->edges[v->currentEdgeIndex];

        // --- SPEED LOGIC ---
        float maxEdgeSpeed = ((float)currentEdge.maxSpeed) * 0.35f; 
        if(maxEdgeSpeed < 4.0f) maxEdgeSpeed = 4.0f; 
        
        float targetSpeed = maxEdgeSpeed;
        
        // Smart Turn Logic
        float distRemaining = v->edgeLength * (1.0f - v->progress);
        
        if (distRemaining < 25.0f) { // Approaching intersection
            if (v->nextEdgeIndex != -1) {
                Vector2 s1 = map->nodes[v->startNodeID].position;
                Vector2 e1 = map->nodes[v->endNodeID].position;
                Vector2 dir1 = Vector2Normalize(Vector2Subtract(e1, s1));

                Edge nextE = map->edges[v->nextEdgeIndex];
                Vector2 s2 = map->nodes[nextE.startNode].position;
                Vector2 e2 = map->nodes[nextE.endNode].position;
                
                Vector2 dir2;
                if (nextE.startNode == v->endNodeID) {
                    dir2 = Vector2Normalize(Vector2Subtract(e2, s2));
                } else {
                    dir2 = Vector2Normalize(Vector2Subtract(s2, e2));
                }

                float turnSharpness = Vector2DotProduct(dir1, dir2);
                
                if (turnSharpness < 0.9f) {
                    float turnSpeed = 6.0f; 
                    float blend = distRemaining / 25.0f; 
                    float approachSpeed = Lerp(turnSpeed, maxEdgeSpeed, blend);
                    
                    if (approachSpeed < targetSpeed) targetSpeed = approachSpeed;
                }
            } 
            else {
                // Dead End
                float blend = distRemaining / 25.0f;
                targetSpeed = Lerp(0.0f, maxEdgeSpeed, blend);
            }
        }

        // Obstacle detection (Car ahead)
        float distToCar = GetDistanceToCarAhead(traffic, i, v->position, v->forward, v->currentEdgeIndex);
        if (distToCar != -1.0f) {
            if (distToCar < STOP_DISTANCE) targetSpeed = 0.0f; 
            else {
                float factor = (distToCar - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                targetSpeed = fminf(targetSpeed, maxEdgeSpeed * factor);
            }
        }

        // Obstacle detection (Player)
        float distToPlayer = GetDistanceToPlayer(v->position, v->forward, player_position);
        if (distToPlayer != -1.0f) {
            if (distToPlayer < STOP_DISTANCE) targetSpeed = 0.0f; 
            else {
                float factor = (distToPlayer - STOP_DISTANCE) / (DETECTION_DIST - STOP_DISTANCE);
                float playerTarget = maxEdgeSpeed * factor;
                if (playerTarget < targetSpeed) targetSpeed = playerTarget;
            }
        }

        // Apply Speed
        v->speed = Lerp(v->speed, targetSpeed, ((v->speed > targetSpeed) ? BRAKE_RATE : ACCEL_RATE) * dt);

        // Stuck removal
        if (v->speed < 0.2f) {
             v->stuckTimer += dt;
             if (v->stuckTimer > STUCK_THRESHOLD) v->active = false; 
        } else v->stuckTimer = 0.0f;

        // --- MOVEMENT MATH ---
        v->progress += (v->speed * dt) / v->edgeLength;

        // Check if segment completed
        if (v->progress >= 1.0f) {
            int nextEdge = v->nextEdgeIndex;
            if (nextEdge == -1) nextEdge = v->currentEdgeIndex; 

            v->currentEdgeIndex = nextEdge;
            v->startNodeID = v->endNodeID; 
            
            Edge nextE = map->edges[v->currentEdgeIndex];
            v->endNodeID = (nextE.startNode == v->startNodeID) ? nextE.endNode : nextE.startNode;
            
            v->progress = 0.0f;
            v->nextEdgeIndex = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
            
            v->edgeLength = Vector2Distance(map->nodes[v->startNodeID].position, map->nodes[v->endNodeID].position);
            currentEdge = nextE; 
        }

        // --- LANE ALIGNMENT ---
        Vector2 s2d = map->nodes[v->startNodeID].position;
        Vector2 e2d = map->nodes[v->endNodeID].position;
        
        Vector2 roadDir2D = Vector2Normalize(Vector2Subtract(e2d, s2d));
        v->forward = (Vector3){ roadDir2D.x, 0, roadDir2D.y };

        Vector2 centerPos2D = Vector2Lerp(s2d, e2d, v->progress);
        Vector2 rightVec2D = { -roadDir2D.y, roadDir2D.x };
        
        float offsetVal = currentEdge.oneway ? 0.0f : (currentEdge.width * 0.25f);
        
        v->position.x = centerPos2D.x + (rightVec2D.x * offsetVal);
        v->position.z = centerPos2D.y + (rightVec2D.y * offsetVal);
        v->position.y = ROAD_HEIGHT; 
    }
}

/*
 * Description: Renders all active vehicles as simple 3D block models (sedan, van, truck).
 * Parameters:
 * - traffic: Pointer to TrafficManager.
 * Returns: None.
 */
void DrawTraffic(TrafficManager *traffic) {
    float time = (float)GetTime();

    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        // 1. Calculate the standard road angle
        float roadAngle = (atan2f(v->forward.x, v->forward.z) * RAD2DEG);

        // 2. Selection of Car Type
        int type = i % 3; 
        Vector3 chassisSize, cabinSize;
        float cabinYOffset, cabinZOffset;

        if (type == 1) { // Van
            chassisSize = (Vector3){ 0.75f, 0.4f, 1.5f };
            cabinSize   = (Vector3){ 0.65f, 0.4f, 1.1f };
            cabinYOffset = 0.35f; cabinZOffset = 0.1f;
        } else if (type == 2) { // Long Truck
            chassisSize = (Vector3){ 0.7f, 0.35f, 1.9f };
            cabinSize   = (Vector3){ 0.6f, 0.4f, 0.6f };
            cabinYOffset = 0.35f; cabinZOffset = 0.5f;
        } else { // Sedan
            chassisSize = (Vector3){ 0.7f, 0.35f, 1.3f };
            cabinSize   = (Vector3){ 0.6f, 0.3f, 0.7f };
            cabinYOffset = 0.3f; cabinZOffset = -0.1f;
        }

        rlPushMatrix();
            // A. Move to world position
            rlTranslatef(v->position.x, v->position.y, v->position.z);
            
            // B. Rotate to match road direction
            rlRotatef(roadAngle, 0, 1, 0); 

            // C. Rotate the "Model" 10 degrees clockwise for alignment fix
            rlRotatef(-10.0f, 0, 1, 0); 

            // 1. Chassis
            DrawCube((Vector3){0, 0, 0}, chassisSize.x, chassisSize.y, chassisSize.z, v->color);
            DrawCubeWires((Vector3){0, 0, 0}, chassisSize.x, chassisSize.y, chassisSize.z, DARKGRAY);

            // 2. Cabin
            Vector3 localCabinPos = { 0.0f, cabinYOffset, cabinZOffset };
            DrawCube(localCabinPos, cabinSize.x, cabinSize.y, cabinSize.z, Fade(v->color, 0.8f));
            DrawCubeWires(localCabinPos, cabinSize.x, cabinSize.y, cabinSize.z, DARKGRAY);

            // 3. Windshield
            Vector3 localGlassPos = { 0.0f, cabinYOffset, (cabinSize.z * 0.45f) + cabinZOffset };
            DrawCube(localGlassPos, cabinSize.x * 1.02f, cabinSize.y * 0.6f, 0.05f, (Color){ 100, 180, 255, 180 });

            // 4. Lights
            float backZ = -chassisSize.z * 0.5f;
            float frontZ = chassisSize.z * 0.5f;

            // Brake Lights
            if (v->speed < 3.0f) {
                DrawCube((Vector3){-0.25f, 0.05f, backZ}, 0.15f, 0.1f, 0.05f, RED);
                DrawCube((Vector3){ 0.25f, 0.05f, backZ}, 0.15f, 0.1f, 0.05f, RED);
            }

            // Headlights
            DrawCube((Vector3){-0.25f, 0.0f, frontZ}, 0.2f, 0.15f, 0.02f, RAYWHITE);
            DrawCube((Vector3){ 0.25f, 0.0f, frontZ}, 0.2f, 0.15f, 0.02f, RAYWHITE);

        rlPopMatrix();
    }
}

/*
 * Description: Checks if the player has collided with any active traffic vehicle.
 * Parameters:
 * - traffic: Pointer to TrafficManager.
 * - playerX, playerZ: Player position coordinates.
 * - playerRadius: Player collision radius.
 * Returns: Vector3 where X,Y is the push direction and Z is the impact speed (-1 if no collision).
 */
Vector3 TrafficCollision(TrafficManager *traffic, float playerX, float playerZ, float playerRadius) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        float dx = playerX - v->position.x;
        float dz = playerZ - v->position.z;
        float distSq = dx*dx + dz*dz;
        
        float minDist = 1.4f; 
        
        if (distSq < minDist * minDist) {
            Vector2 pushDir = Vector2Normalize((Vector2){dx, dz});
            float speed = v->speed;
            
            // Slow car down on impact
            v->speed *= 0.5f; 
            
            // Return X=PushX, Y=PushZ, Z=ImpactSpeed
            return (Vector3){pushDir.x, pushDir.y, speed};
        }
    }
    return (Vector3){0,0,-1};
}
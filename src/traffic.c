#include "traffic.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define SPAWN_RATE 0.1f       // Try to spawn frequently
#define SPAWN_RADIUS_MIN 40.0f
#define SPAWN_RADIUS_MAX 120.0f
#define DESPAWN_RADIUS 150.0f

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
    }
    traffic->spawnTimer = 0.0f;
}

// --- Helper: Find a new road connected to 'nodeID' ---
// Returns the index of a valid edge, or -1 if dead end
// 'excludeEdgeIndex' prevents U-turns immediately (unless dead end)
int FindNextEdge(GameMap *map, int nodeID, int excludeEdgeIndex) {
    int candidates[10]; // Support up to 10-way intersections
    int count = 0;

    for (int i = 0; i < map->edgeCount; i++) {
        if (i == excludeEdgeIndex) continue;

        // Is this edge connected to our current node?
        if (map->edges[i].startNode == nodeID || map->edges[i].endNode == nodeID) {
            candidates[count++] = i;
            if (count >= 10) break;
        }
    }

    if (count > 0) {
        return candidates[GetRandomValue(0, count - 1)];
    }
    
    // Dead end? Forced U-turn (go back the way we came)
    return excludeEdgeIndex; 
}

void UpdateTraffic(TrafficManager *traffic, Player *player, GameMap *map, float dt) {
    if (map->edgeCount == 0) return;

    // --- 1. SPAWN LOGIC ---
    traffic->spawnTimer += dt;
    if (traffic->spawnTimer > SPAWN_RATE) {
        traffic->spawnTimer = 0.0f;

        // Find free slot
        int slot = -1;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (!traffic->vehicles[i].active) {
                slot = i;
                break;
            }
        }

        if (slot != -1) {
            // Pick a random edge
            int edgeIdx = GetRandomValue(0, map->edgeCount - 1);
            Edge e = map->edges[edgeIdx];
            
            // Check if valid edge
            if (e.startNode < map->nodeCount && e.endNode < map->nodeCount) {
                Vector3 p1 = { map->nodes[e.startNode].position.x, 0, map->nodes[e.startNode].position.y };
                Vector3 p2 = { map->nodes[e.endNode].position.x, 0, map->nodes[e.endNode].position.y };
                Vector3 mid = Vector3Scale(Vector3Add(p1, p2), 0.5f);

                float distToPlayer = Vector3Distance(mid, player->position);

                // Goldilocks Zone Check
                if (distToPlayer > SPAWN_RADIUS_MIN && distToPlayer < SPAWN_RADIUS_MAX) {
                    Vehicle *v = &traffic->vehicles[slot];
                    v->active = true;
                    v->currentEdgeIndex = edgeIdx;
                    
                    // Randomize Direction (Start->End or End->Start)
                    if (GetRandomValue(0, 1) == 0) {
                        v->startNodeID = e.startNode;
                        v->endNodeID = e.endNode;
                    } else {
                        v->startNodeID = e.endNode;
                        v->endNodeID = e.startNode;
                    }

                    v->progress = GetRandomValue(10, 90) / 100.0f; // Random spot on road
                    v->speed = TRAFFIC_BASE_SPEED + ((float)GetRandomValue(-100, 100) / 100.0f) * TRAFFIC_SPEED_VAR;
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

        // Despawn check
        if (Vector3Distance(v->position, player->position) > DESPAWN_RADIUS) {
            v->active = false;
            continue;
        }

        // --- MOVEMENT ---
        // 1. Calculate step based on speed and edge length
        float moveStep = (v->speed * dt) / v->edgeLength;
        v->progress += moveStep;

        // 2. Check if reached end of road (Intersection)
        if (v->progress >= 1.0f) {
            // Arrived at v->endNodeID. Find new path.
            int nextEdgeIdx = FindNextEdge(map, v->endNodeID, v->currentEdgeIndex);
            
            if (nextEdgeIdx != -1) {
                Edge nextEdge = map->edges[nextEdgeIdx];
                
                // Update State for new road
                v->currentEdgeIndex = nextEdgeIdx;
                v->startNodeID = v->endNodeID; // We start where we finished
                
                // The new end is whichever node of the new edge isn't the start
                if (nextEdge.startNode == v->startNodeID) v->endNodeID = nextEdge.endNode;
                else v->endNodeID = nextEdge.startNode;

                // Reset progress
                v->progress = 0.0f;
                
                // Recalculate length
                Vector3 pStart = { map->nodes[v->startNodeID].position.x, 0, map->nodes[v->startNodeID].position.y };
                Vector3 pEnd = { map->nodes[v->endNodeID].position.x, 0, map->nodes[v->endNodeID].position.y };
                v->edgeLength = Vector3Distance(pStart, pEnd);

            } else {
                // Should technically imply a dead end with no U-turn possible (rare)
                // Just flip direction on same road
                int temp = v->startNodeID;
                v->startNodeID = v->endNodeID;
                v->endNodeID = temp;
                v->progress = 0.0f;
            }
        }

        // --- POSITION CALCULATION (Lane Offset) ---
        // We need to calculate world position based on progress
        
        Vector2 startNode2D = map->nodes[v->startNodeID].position;
        Vector2 endNode2D = map->nodes[v->endNodeID].position;

        Vector3 startPos = { startNode2D.x, 0.0f, startNode2D.y };
        Vector3 endPos = { endNode2D.x, 0.0f, endNode2D.y };

        // Basic Lerp
        Vector3 currentPos = Vector3Lerp(startPos, endPos, v->progress);

        // Calculate Lane Offset (Right-Hand Drive)
        // 1. Direction Vector
        Vector3 dir = Vector3Normalize(Vector3Subtract(endPos, startPos));
        v->forward = dir; // Store for drawing

        // 2. Right Vector (Cross product with Up)
        Vector3 right = Vector3CrossProduct((Vector3){0,1,0}, dir);
        
        // 3. Offset Amount (1/4th of width roughly puts it in the right lane)
        float laneOffset = map->edges[v->currentEdgeIndex].width * 0.25f;
        
        // Apply Offset
        v->position = Vector3Add(currentPos, Vector3Scale(right, laneOffset));
        
        // Height bias
        v->position.y = 0.5f; 
    }
}

void DrawTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (v->active) {
            // Draw Car
            // We use the 'forward' vector to calculate rotation angle
            float angle = atan2f(v->forward.x, v->forward.z) * RAD2DEG;
            
            Vector3 size = { 1.0f, 0.8f, 2.0f }; // Width, Height, Length
            
            // DrawCube doesn't support rotation easily, so we use DrawModel or wires
            // Or use DrawCubeWires with a push matrix. 
            // For simple prototype, lets manually rotate corners or just draw a Sphere for now?
            // Actually, let's use a specialized function or just an aligned box if rotation is hard.
            // Since we want rotation, let's use DrawCube but aligned to axis? No, looks bad.
            
            // Better: Use DrawModelEx with a built-in cube model if available, 
            // or just draw a sphere/cylinder which doesn't need rotation visually as much.
            
            // For this specific prototype, let's assume we can draw an oriented box manually
            // OR just draw a simple colored sphere for now until we import a car model.
            // DrawSphere(v->position, 1.0f, v->color);
            
            // Alternative: Draw Oriented Rectangle using Geometric math
            Vector3 pos = v->position;
            Vector3 forward = Vector3Scale(v->forward, 1.0f); // Half length
            Vector3 right = Vector3Scale(Vector3CrossProduct((Vector3){0,1,0}, v->forward), 0.5f); // Half width
            
            // Car is roughly a box
            // We can draw 2 lines to show direction
            DrawCube(pos, 0.8f, 0.8f, 0.8f, v->color); // Center cabin
            DrawLine3D(pos, Vector3Add(pos, Vector3Scale(forward, 2.0f)), BLACK); // Direction pointer
        }
    }
}
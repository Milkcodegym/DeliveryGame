#include "traffic.h"
#include "raymath.h"
#include <stdlib.h>

#define INNER_RADIUS 10.0f
#define OUTER_RADIUS 40.0f
#define SPAWN_RATE 0.5f

void InitTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        traffic->vehicles[i].active = false;
    }
    traffic->spawnTimer = 0.0f;
}

void UpdateTraffic(TrafficManager *traffic, Player *player, GameMap *map, float dt) {
    // 1. Spawning Logic (The "Donut")
    traffic->spawnTimer += dt;
    if (traffic->spawnTimer > SPAWN_RATE) {
        traffic->spawnTimer = 0.0f;

        // Try to find a free slot
        int slot = -1;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (!traffic->vehicles[i].active) {
                slot = i;
                break;
            }
        }

        if (slot != -1 && map->width > 0) {
            // Try multiple times to find a valid road
            for (int attempt = 0; attempt < 10; attempt++) {
                // Pick random spot in Goldilocks Zone
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float dist = (float)GetRandomValue((int)INNER_RADIUS, (int)OUTER_RADIUS);
                
                int tx = (int)(player->position.x + cosf(angle) * dist);
                int ty = (int)(player->position.z + sinf(angle) * dist);

                if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                    int idx = ty * map->width + tx;
                    if (map->roads[idx].isRoad) {
                        // Found a road, spawn car
                        Vehicle *v = &traffic->vehicles[slot];
                        
                        // Pick initial valid direction
                        bool foundDir = false;
                        for (int d = 0; d < 4; d++) {
                            if (map->roads[idx].connects[d]) {
                                v->direction = d;
                                v->target = map->roads[idx].waypoints[d];
                            v->target.y = 0.2f; // Ensure target is at car height
                                foundDir = true;
                                break;
                            }
                        }

                        if (foundDir) {
                            v->active = true;
                            v->position = (Vector3){ (float)tx, 0.2f, (float)ty };
                            v->speed = (float)GetRandomValue(30, 50) / 10.0f; // 3.0 to 5.0
                            v->color = (Color){ GetRandomValue(50, 255), GetRandomValue(50, 255), GetRandomValue(50, 255), 255 };
                            break; // Stop attempting, we spawned one
                        }
                    }
                }
            }
        }
    }

    // 2. Update Vehicles
    for (int i = 0; i < MAX_VEHICLES; i++) {
        Vehicle *v = &traffic->vehicles[i];
        if (!v->active) continue;

        // Despawn if too far
        float distToPlayer = Vector3Distance(v->position, player->position);
        if (distToPlayer > OUTER_RADIUS + 5.0f) {
            v->active = false;
            continue;
        }

        // --- AI BEHAVIOR ---
        
        // Raycast / Braking
        float currentSpeed = v->speed;
        Vector3 forward = Vector3Normalize(Vector3Subtract(v->target, v->position));
        
        // Check against Player
        if (Vector3Distance(v->position, player->position) < 4.0f) {
            Vector3 toPlayer = Vector3Normalize(Vector3Subtract(player->position, v->position));
            if (Vector3DotProduct(forward, toPlayer) > 0.7f) {
                currentSpeed = 0.0f; // Hard Stop
            }
        }

        // Check against other cars
        for (int j = 0; j < MAX_VEHICLES; j++) {
            if (i == j || !traffic->vehicles[j].active) continue;
            if (Vector3Distance(v->position, traffic->vehicles[j].position) < 2.5f) {
                Vector3 toCar = Vector3Normalize(Vector3Subtract(traffic->vehicles[j].position, v->position));
                if (Vector3DotProduct(forward, toCar) > 0.7f) {
                    currentSpeed = 0.0f; // Stop for car ahead
                }
            }
        }

        // Move
        float step = currentSpeed * dt;
        v->position.x += forward.x * step;
        v->position.z += forward.z * step;

        // Reached Target?
        if (Vector3Distance(v->position, v->target) < 0.5f) {
            // Snap to target
            v->position = v->target;
            v->position.y = 0.2f; // Maintain height

            // Determine current tile
            int tx = (int)(v->position.x + 0.5f);
            int ty = (int)(v->position.z + 0.5f);
            
            if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                RoadTile tile = map->roads[ty * map->width + tx];
                
                // Pick next direction
                // Try to go straight, or turn if straight not available
                int validDirs[4];
                int count = 0;
                
                // Prevent U-Turn unless dead end
                int opposite = (v->direction + 2) % 4;

                for (int d = 0; d < 4; d++) {
                    if (tile.connects[d] && d != opposite) {
                        validDirs[count++] = d;
                    }
                }

                // Dead end? Allow U-turn
                if (count == 0 && tile.connects[opposite]) {
                    validDirs[count++] = opposite;
                }

                if (count > 0) {
                    int pick = GetRandomValue(0, count - 1);
                    v->direction = validDirs[pick];
                    
                    // Set new target: The waypoint of the NEXT tile in that direction
                    // Actually, we just aim for the waypoint of the CURRENT tile's exit?
                    // No, we are AT the current tile. We need to move to the neighbor.
                    // The neighbor's waypoint for THIS direction is the target.
                    int dx[] = {0, 1, 0, -1};
                    int dy[] = {-1, 0, 1, 0};
                    int nx = tx + dx[v->direction];
                    int ny = ty + dy[v->direction];
                    
                    // Offsets for Right-Hand Rule (Must match map.c)
                    float offX[] = {0.2f, 0.0f, -0.2f, 0.0f};
                    float offZ[] = {0.0f, 0.2f, 0.0f, -0.2f};

                    if (nx >= 0 && nx < map->width && ny >= 0 && ny < map->height) {
                        v->target = map->roads[ny * map->width + nx].waypoints[v->direction];
                        // Calculate target manually to ensure it's valid even if the neighbor turns
                        v->target.x = (float)nx + offX[v->direction];
                        v->target.z = (float)ny + offZ[v->direction];
                        v->target.y = 0.2f;
                    }
                }
            }
        }
    }
}

void DrawTraffic(TrafficManager *traffic) {
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (traffic->vehicles[i].active) {
            DrawCube(traffic->vehicles[i].position, 0.6f, 0.5f, 1.0f, traffic->vehicles[i].color);
        }
    }
}
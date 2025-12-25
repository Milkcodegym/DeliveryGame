#include "map.h"
#include "raymath.h"
#include "rlgl.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h> 

const float MAP_SCALE = 0.4f; 
const float RENDER_DISTANCE = 200.0f; 

// --- SEARCH IMPLEMENTATION ---
int SearchLocations(GameMap *map, const char* query, MapLocation* results) {
    if (strlen(query) == 0 || map->locationCount == 0) return 0;
    
    int count = 0;
    for (int i = 0; i < map->locationCount; i++) {
        if (map->locations[i].type == LOC_HOUSE) continue;

        if (strstr(map->locations[i].name, query) != NULL) {
            results[count] = map->locations[i];
            count++;
            if (count >= MAX_SEARCH_RESULTS) break;
        }
    }
    return count;
}

// --- VISUAL EFFECTS ---
void UpdateMapEffects(GameMap *map, Vector3 playerPos) {
    for (int i = 0; i < map->locationCount; i++) {
        Vector3 locPos = { map->locations[i].position.x, 0.5f, map->locations[i].position.y };
        float dist = Vector3Distance(playerPos, locPos);
        
        if (dist < 50.0f) {
            float pulse = (sinf(GetTime() * 5.0f) + 1.0f) * 0.5f; 
            float radius = 4.0f + pulse; 
            
            DrawCircle3D(locPos, radius, (Vector3){1,0,0}, 90.0f, Fade(RED, 0.3f));
            DrawCircle3D(locPos, radius * 0.8f, (Vector3){1,0,0}, 90.0f, Fade(YELLOW, 0.3f));
            DrawCubeWires(locPos, 2.0f, 10.0f, 2.0f, Fade(GOLD, 0.5f));
        }
    }
}

// --- HELPER FUNCTIONS ---

Vector3 CalculateWallNormal(Vector2 p1, Vector2 p2) {
    Vector2 dir = Vector2Subtract(p2, p1);
    Vector2 normal2D = { -dir.y, dir.x };
    normal2D = Vector2Normalize(normal2D);
    return (Vector3){ normal2D.x, 0.0f, normal2D.y };
}

Vector2 GetBuildingCenter(Vector2 *footprint, int count) {
    Vector2 center = {0};
    if (count == 0) return center;
    for(int i = 0; i < count; i++) {
        center = Vector2Add(center, footprint[i]);
    }
    center.x /= count;
    center.y /= count;
    return center;
}

void DrawRoadSegment(Vector3 start, Vector3 end, float width) {
    Vector3 diff = Vector3Subtract(end, start);
    if (Vector3Length(diff) < 0.001f) return;

    Vector3 right = Vector3CrossProduct((Vector3){0,1,0}, diff);
    right = Vector3Normalize(right);
    right = Vector3Scale(right, width * 0.5f);

    float yLayer = 0.02f; 

    Vector3 v1 = Vector3Subtract(start, right); v1.y = yLayer; 
    Vector3 v2 = Vector3Add(start, right);      v2.y = yLayer; 
    Vector3 v3 = Vector3Add(end, right);        v3.y = yLayer; 
    Vector3 v4 = Vector3Subtract(end, right);   v4.y = yLayer; 

    DrawTriangle3D(v1, v3, v2, DARKGRAY); 
    DrawTriangle3D(v1, v2, v3, DARKGRAY); 
    DrawTriangle3D(v1, v4, v3, DARKGRAY); 
    DrawTriangle3D(v1, v3, v4, DARKGRAY); 

    // Caps
    DrawCylinder((Vector3){start.x, yLayer, start.z}, width * 0.5f, width * 0.5f, 0.02f, 8, DARKGRAY);
    DrawCylinder((Vector3){end.x, yLayer, end.z}, width * 0.5f, width * 0.5f, 0.02f, 8, DARKGRAY);
}

// --- OPTIMIZED BATCH GENERATION ---

// Helper to push a single building's geometry into the master arrays
void AppendBuildingToBatch(Building *b, float *verts, float *norms, unsigned char *colors, int *vIdx) {
    int v = *vIdx;
    Color c = b->color;

    // 1. WALLS
    for (int i = 0; i < b->pointCount; i++) {
        Vector2 p1 = b->footprint[i];
        Vector2 p2 = b->footprint[(i + 1) % b->pointCount];
        Vector3 normal = CalculateWallNormal(p1, p2);

        // Quad (2 triangles) per wall segment
        // We define the 6 vertices explicitly to control normals/colors per face
        
        // Triangle 1
        verts[v*3+0] = p1.x; verts[v*3+1] = 0;         verts[v*3+2] = p1.y;
        verts[v*3+3] = p1.x; verts[v*3+4] = b->height; verts[v*3+5] = p1.y;
        verts[v*3+6] = p2.x; verts[v*3+7] = 0;         verts[v*3+8] = p2.y;
        
        // Triangle 2
        verts[v*3+9]  = p1.x; verts[v*3+10] = b->height; verts[v*3+11] = p1.y;
        verts[v*3+12] = p2.x; verts[v*3+13] = b->height; verts[v*3+14] = p2.y;
        verts[v*3+15] = p2.x; verts[v*3+16] = 0;         verts[v*3+17] = p2.y;

        // Normals & Colors for these 6 vertices
        for(int k=0; k<6; k++) {
            int idx = v + k;
            norms[idx*3+0] = normal.x; norms[idx*3+1] = normal.y; norms[idx*3+2] = normal.z;
            colors[idx*4+0] = c.r; colors[idx*4+1] = c.g; colors[idx*4+2] = c.b; colors[idx*4+3] = 255;
        }
        v += 6;
    }

    // 2. ROOF (Fan triangulation)
    // Note: Assumes convex polygons. If buildings are concave, this is simple approximation.
    for (int i = 0; i < b->pointCount - 2; i++) {
        Vector2 p0 = b->footprint[0];
        Vector2 p1 = b->footprint[i + 1];
        Vector2 p2 = b->footprint[i + 2];
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        
        verts[v*3+0] = p0.x; verts[v*3+1] = b->height; verts[v*3+2] = p0.y;
        verts[v*3+3] = p1.x; verts[v*3+4] = b->height; verts[v*3+5] = p1.y;
        verts[v*3+6] = p2.x; verts[v*3+7] = b->height; verts[v*3+8] = p2.y;

        for(int k=0; k<3; k++) {
            int idx = v + k;
            norms[idx*3+0] = up.x; norms[idx*3+1] = up.y; norms[idx*3+2] = up.z;
            colors[idx*4+0] = c.r; colors[idx*4+1] = c.g; colors[idx*4+2] = c.b; colors[idx*4+3] = 255;
        }
        v += 3;
    }
    
    *vIdx = v;
}

void GenerateMapBatch(GameMap *map) {
    if (map->buildingCount == 0 || map->isBatchLoaded) return;
    
    printf("Optimizing Map: Batching %d buildings...\n", map->buildingCount);

    // 1. Calculate size needed
    int totalVerts = 0;
    for(int i=0; i<map->buildingCount; i++) {
        int walls = map->buildings[i].pointCount * 6; // 6 verts per wall segment
        int roof = (map->buildings[i].pointCount - 2) * 3; // 3 verts per roof tri
        totalVerts += (walls + roof);
    }

    // 2. Allocate the BIG mesh
    Mesh mesh = { 0 };
    mesh.vertexCount = totalVerts;
    mesh.triangleCount = totalVerts / 3;
    
    mesh.vertices = (float *)malloc(totalVerts * 3 * sizeof(float));
    mesh.normals = (float *)malloc(totalVerts * 3 * sizeof(float));
    mesh.colors = (unsigned char *)malloc(totalVerts * 4 * sizeof(unsigned char));
    mesh.texcoords = (float *)calloc(totalVerts * 2, sizeof(float)); // Zeroed out, not using textures yet

    // 3. Fill the mesh
    int vIndex = 0;
    for(int i=0; i<map->buildingCount; i++) {
        AppendBuildingToBatch(&map->buildings[i], mesh.vertices, mesh.normals, mesh.colors, &vIndex);
    }

    // 4. Upload and create model
    UploadMesh(&mesh, false);
    map->cityBatch = LoadModelFromMesh(mesh);
    
    // IMPORTANT: Set material to white so Vertex Colors (mesh.colors) show through correctly
    map->cityBatch.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    
    map->isBatchLoaded = true;
    printf("Map Optimized. Total Vertices: %d\n", totalVerts);
}

// --- CORE MAP FUNCTIONS ---

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));
    map.areas = (MapArea *)calloc(MAX_AREAS, sizeof(MapArea));
    map.isBatchLoaded = false;

    char *text = LoadFileText(fileName);
    if (!text) {
        printf("CRITICAL ERROR: Could not load map file %s\n", fileName);
        return map;
    }

    char *line = strtok(text, "\n");
    int mode = 0; 

    while (line != NULL) {
        if (strncmp(line, "NODES:", 6) == 0) { mode = 1; }
        else if (strncmp(line, "EDGES:", 6) == 0) { mode = 2; }
        else if (strncmp(line, "BUILDINGS:", 10) == 0) { mode = 3; }
        else if (strncmp(line, "AREAS:", 6) == 0) { mode = 4; }
        else if (strncmp(line, "L ", 2) == 0) { 
             if (map.locationCount < MAX_LOCATIONS) {
                 int type; float x, y; char name[64];
                 if (sscanf(line, "L %d %f %f %63s", &type, &x, &y, name) == 4) {
                     map.locations[map.locationCount].position = (Vector2){ x * MAP_SCALE, y * MAP_SCALE };
                     map.locations[map.locationCount].type = (LocationType)type;
                     map.locations[map.locationCount].iconID = type;
                     for(int k=0; name[k]; k++) if(name[k] == '_') name[k] = ' ';
                     strncpy(map.locations[map.locationCount].name, name, 64);
                     map.locationCount++;
                 }
             }
        }
        else {
            if (mode == 1 && map.nodeCount < MAX_NODES) {
                int id; float x, y; int flags;
                if (sscanf(line, "%d: %f %f %d", &id, &x, &y, &flags) >= 3) {
                    map.nodes[map.nodeCount].id = id;
                    map.nodes[map.nodeCount].position = (Vector2){x * MAP_SCALE, y * MAP_SCALE};
                    map.nodeCount++;
                }
            } else if (mode == 2 && map.edgeCount < MAX_EDGES) {
                int start, end, oneway, speed, lanes; float width;
                if (sscanf(line, "%d %d %f %d %d %d", &start, &end, &width, &oneway, &speed, &lanes) >= 3) {
                    map.edges[map.edgeCount].startNode = start;
                    map.edges[map.edgeCount].endNode = end;
                    map.edges[map.edgeCount].width = width * MAP_SCALE;
                    map.edgeCount++;
                }
            } else if (mode == 3 && map.buildingCount < MAX_BUILDINGS) {
                float h; int r, g, b;
                char *ptr = line;
                int read = 0;
                Building *build = &map.buildings[map.buildingCount];
                if (sscanf(ptr, "%f %d %d %d%n", &h, &r, &g, &b, &read) == 4) {
                    build->height = h * MAP_SCALE;
                    build->color = (Color){r, g, b, 255};
                    ptr += read;
                    Vector2 tempPoints[MAX_BUILDING_POINTS];
                    int pCount = 0;
                    float px, py;
                    while (sscanf(ptr, "%f %f%n", &px, &py, &read) == 2 && pCount < MAX_BUILDING_POINTS) {
                        tempPoints[pCount] = (Vector2){px * MAP_SCALE, py * MAP_SCALE};
                        pCount++;
                        ptr += read;
                    }
                    build->footprint = (Vector2 *)malloc(sizeof(Vector2) * pCount);
                    memcpy(build->footprint, tempPoints, sizeof(Vector2) * pCount);
                    build->pointCount = pCount;
                    
                    // NOTE: We do NOT generate individual meshes here anymore.
                    
                    if (pCount >= 3) {
                        map.buildingCount++;
                    }
                }
            } else if (mode == 4 && map.areaCount < MAX_AREAS) {
                int type, r, g, b;
                char *ptr = line;
                int read = 0;
                MapArea *area = &map.areas[map.areaCount];
                if (sscanf(ptr, "%d %d %d %d%n", &type, &r, &g, &b, &read) == 4) {
                    area->type = type;
                    area->color = (Color){r, g, b, 255};
                    ptr += read;
                    Vector2 tempPoints[MAX_BUILDING_POINTS];
                    int pCount = 0;
                    float px, py;
                    while (sscanf(ptr, "%f %f%n", &px, &py, &read) == 2 && pCount < MAX_BUILDING_POINTS) {
                        tempPoints[pCount] = (Vector2){px * MAP_SCALE, py * MAP_SCALE};
                        pCount++;
                        ptr += read;
                    }
                    area->points = (Vector2 *)malloc(sizeof(Vector2) * pCount);
                    memcpy(area->points, tempPoints, sizeof(Vector2) * pCount);
                    area->pointCount = pCount;
                    map.areaCount++;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    UnloadFileText(text);
    return map;
}

void UnloadGameMap(GameMap *map) {
    // Unload the batched city model
    if (map->isBatchLoaded) {
        UnloadModel(map->cityBatch);
        map->isBatchLoaded = false;
    }

    if (map->nodes) free(map->nodes);
    if (map->edges) free(map->edges);
    
    for (int i = 0; i < map->buildingCount; i++) {
        // Only free footprint, models are gone
        free(map->buildings[i].footprint);
    }
    for (int i = 0; i < map->areaCount; i++) {
        free(map->areas[i].points);
    }
    if (map->buildings) free(map->buildings);
    if (map->locations) free(map->locations);
    if (map->areas) free(map->areas);
    if (map->graph) {
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }
}

void DrawGameMap(GameMap *map, Vector3 playerPos) {
    rlDisableBackfaceCulling(); 
    Vector2 pPos2D = { playerPos.x, playerPos.z };

    // 1. Draw Areas (Ground)
    for(int i=0; i<map->areaCount; i++) {
        if (map->areas[i].pointCount > 0) {
            // Simple distance check for areas is fine as they are low poly
            if (Vector2Distance(pPos2D, map->areas[i].points[0]) > RENDER_DISTANCE) continue;
        }

        Vector3 center = {0, 0.01f, 0};
        for(int j=0; j<map->areas[i].pointCount; j++) {
            center.x += map->areas[i].points[j].x;
            center.z += map->areas[i].points[j].y;
        }
        center.x /= map->areas[i].pointCount;
        center.z /= map->areas[i].pointCount;

        for(int j=0; j<map->areas[i].pointCount; j++) {
            Vector2 p1 = map->areas[i].points[j];
            Vector2 p2 = map->areas[i].points[(j+1)%map->areas[i].pointCount];
            DrawTriangle3D(center, (Vector3){p2.x, 0.01f, p2.y}, (Vector3){p1.x, 0.01f, p1.y}, map->areas[i].color);
        }
    }

    // 2. Draw Roads
    // Note: Road drawing is still immediate mode, but less heavy than buildings.
    for (int i = 0; i < map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;

        if (Vector2Distance(pPos2D, s) > RENDER_DISTANCE && Vector2Distance(pPos2D, en) > RENDER_DISTANCE) {
            continue; 
        }

        DrawRoadSegment((Vector3){s.x, 0, s.y}, (Vector3){en.x, 0, en.y}, e.width);
    }
    
    // 3. Draw Buildings (OPTIMIZED BATCH)
    // Check if we need to generate the batch (Lazy Loading)
    if (!map->isBatchLoaded) {
        GenerateMapBatch(map);
    }
    
    // Draw the entire city in ONE draw call
    // Note: We don't perform distance checks here because the GPU can cull faster than CPU
    DrawModel(map->cityBatch, (Vector3){0,0,0}, 1.0f, WHITE);
    
    // 4. Draw Locations (POIs)
    for (int i = 0; i < map->locationCount; i++) {
        float dist = Vector2Distance(pPos2D, map->locations[i].position);
        if (dist > RENDER_DISTANCE) continue;

        Vector3 pos = { map->locations[i].position.x, 1.0f, map->locations[i].position.y };
        Color poiColor = RED;
        
        switch(map->locations[i].type) {
            case LOC_FUEL: poiColor = ORANGE; break;
            case LOC_FOOD: poiColor = GREEN; break;
            case LOC_MARKET: poiColor = BLUE; break;
            default: break;
        }

        DrawSphere(pos, 1.5f, Fade(poiColor, 0.8f));
        Vector3 textPos = { pos.x, 4.0f, pos.z };
        DrawLine3D(pos, textPos, BLACK);
    }
    
    rlEnableBackfaceCulling();
}

bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    Vector2 p = { x, z };
    for (int i = 0; i < map->buildingCount; i++) {
        Vector2 center = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        
        // Fast distance check (approx 25.0f covers most building sizes + player size)
        if (Vector2Distance(p, center) > 25.0f) continue; 

        if (CheckCollisionPointPoly(p, map->buildings[i].footprint, map->buildings[i].pointCount)) {
            return true;
        }
    }
    return false;
}

// --- GRAPH & PATHFINDING (UNCHANGED) ---

void BuildMapGraph(GameMap *map) {
    printf("Building Navigation Graph...\n");
    map->graph = (NodeGraph*)calloc(map->nodeCount, sizeof(NodeGraph));

    for (int i = 0; i < map->edgeCount; i++) {
        int u = map->edges[i].startNode;
        int v = map->edges[i].endNode;
        if (u >= map->nodeCount || v >= map->nodeCount) continue;

        float dist = Vector2Distance(map->nodes[u].position, map->nodes[v].position);

        if (map->graph[u].count >= map->graph[u].capacity) {
            map->graph[u].capacity = (map->graph[u].capacity == 0) ? 4 : map->graph[u].capacity * 2;
            map->graph[u].connections = realloc(map->graph[u].connections, map->graph[u].capacity * sizeof(GraphConnection));
        }
        map->graph[u].connections[map->graph[u].count].targetNodeIndex = v;
        map->graph[u].connections[map->graph[u].count].distance = dist;
        map->graph[u].count++;

        if (map->graph[v].count >= map->graph[v].capacity) {
            map->graph[v].capacity = (map->graph[v].capacity == 0) ? 4 : map->graph[v].capacity * 2;
            map->graph[v].connections = realloc(map->graph[v].connections, map->graph[v].capacity * sizeof(GraphConnection));
        }
        map->graph[v].connections[map->graph[v].count].targetNodeIndex = u;
        map->graph[v].connections[map->graph[v].count].distance = dist;
        map->graph[v].count++;
    }
}

int GetClosestNode(GameMap *map, Vector2 position) {
    int bestNode = -1;
    float minDst = FLT_MAX;
    
    // Pass 1: Prioritize connected nodes
    for (int i = 0; i < map->nodeCount; i++) {
        if (map->graph && map->graph[i].count == 0) continue; 
        float d = Vector2DistanceSqr(position, map->nodes[i].position);
        if (d < minDst) {
            minDst = d;
            bestNode = i;
        }
    }
    
    // Pass 2: Fallback
    if (bestNode == -1) {
        minDst = FLT_MAX;
        for (int i = 0; i < map->nodeCount; i++) {
            float d = Vector2DistanceSqr(position, map->nodes[i].position);
            if (d < minDst) {
                minDst = d;
                bestNode = i;
            }
        }
    }
    return bestNode;
}

int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen) {
    if (!map->graph) BuildMapGraph(map);

    int startNode = GetClosestNode(map, startPos);
    int endNode = GetClosestNode(map, endPos);
    if (startNode == -1 || endNode == -1) return 0;
    if (startNode == endNode) return 0;

    float *gScore = malloc(map->nodeCount * sizeof(float));
    float *fScore = malloc(map->nodeCount * sizeof(float));
    int *comeFrom = malloc(map->nodeCount * sizeof(int));
    bool *inOpenSet = calloc(map->nodeCount, sizeof(bool)); 
    
    for(int i=0; i<map->nodeCount; i++) {
        gScore[i] = FLT_MAX;
        fScore[i] = FLT_MAX;
        comeFrom[i] = -1;
    }

    int *openList = malloc(map->nodeCount * sizeof(int));
    int openCount = 0;

    gScore[startNode] = 0;
    fScore[startNode] = Vector2Distance(map->nodes[startNode].position, map->nodes[endNode].position);
    openList[openCount++] = startNode;
    inOpenSet[startNode] = true;

    int found = 0;
    while (openCount > 0) {
        int lowestIdx = 0;
        for(int i=1; i<openCount; i++) {
            if (fScore[openList[i]] < fScore[openList[lowestIdx]]) lowestIdx = i;
        }
        int current = openList[lowestIdx];
        if (current == endNode) { found = 1; break; }

        openList[lowestIdx] = openList[openCount - 1];
        openCount--;
        inOpenSet[current] = false;

        for (int i = 0; i < map->graph[current].count; i++) {
            int neighbor = map->graph[current].connections[i].targetNodeIndex;
            float weight = map->graph[current].connections[i].distance;
            float tentative_g = gScore[current] + weight;

            if (tentative_g < gScore[neighbor]) {
                comeFrom[neighbor] = current;
                gScore[neighbor] = tentative_g;
                fScore[neighbor] = gScore[neighbor] + Vector2Distance(map->nodes[neighbor].position, map->nodes[endNode].position);
                if (!inOpenSet[neighbor]) {
                    openList[openCount++] = neighbor;
                    inOpenSet[neighbor] = true;
                }
            }
        }
    }

    int pathLen = 0;
    if (found) {
        int curr = endNode;
        Vector2 *tempPath = malloc(maxPathLen * sizeof(Vector2));
        int count = 0;
        while (curr != -1 && count < maxPathLen) {
            tempPath[count++] = map->nodes[curr].position;
            curr = comeFrom[curr];
        }
        for(int i=0; i<count; i++) outPath[i] = tempPath[count - 1 - i];
        pathLen = count;
        free(tempPath);
    }

    free(gScore); free(fScore); free(comeFrom); free(inOpenSet); free(openList);
    return pathLen;
}
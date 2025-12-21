#include "map.h"
#include "raymath.h"
#include "rlgl.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h> 

const float MAP_SCALE = 0.25f; 

// --- SEARCH IMPLEMENTATION ---
int SearchLocations(GameMap *map, const char* query, MapLocation* results) {
    if (strlen(query) == 0 || map->locationCount == 0) return 0;
    
    int count = 0;
    for (int i = 0; i < map->locationCount; i++) {
        // Skip houses from search results
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
        
        if (dist < 4.0f) {
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

    // Draw Surface
    DrawTriangle3D(v1, v3, v2, DARKGRAY); 
    DrawTriangle3D(v1, v2, v3, DARKGRAY); 
    DrawTriangle3D(v1, v4, v3, DARKGRAY); 
    DrawTriangle3D(v1, v3, v4, DARKGRAY); 

    // Draw Joints (16 slices for smoothness)
    DrawCylinder((Vector3){start.x, yLayer, start.z}, width * 0.5f, width * 0.5f, 0.02f, 16, DARKGRAY);
    DrawCylinder((Vector3){end.x, yLayer, end.z}, width * 0.5f, width * 0.5f, 0.02f, 16, DARKGRAY);
}

Model GenerateBuildingMesh(Vector2 *footprint, int count, float height, Color color) {
    Mesh mesh = { 0 };
    if (count < 3) return LoadModelFromMesh(mesh); 

    int wallVerts = count * 12; 
    int roofVerts = (count - 2) * 3;
    int floorVerts = (count - 2) * 3; 

    mesh.vertexCount = wallVerts + roofVerts + floorVerts;
    mesh.triangleCount = mesh.vertexCount / 3;
    
    mesh.vertices = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float *)malloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)malloc(mesh.vertexCount * 2 * sizeof(float));
    
    int v = 0; 

    for (int i = 0; i < count; i++) {
        Vector2 p1 = footprint[i];
        Vector2 p2 = footprint[(i + 1) % count];
        Vector3 normal = CalculateWallNormal(p1, p2);

        // A Side
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;

        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = normal.x; mesh.normals[v*3+1] = normal.y; mesh.normals[v*3+2] = normal.z; v++;

        // B Side
        Vector3 invNormal = Vector3Negate(normal);
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
        
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = invNormal.x; mesh.normals[v*3+1] = invNormal.y; mesh.normals[v*3+2] = invNormal.z; v++;
    }

    // Roof & Floor
    for (int i = 0; i < count - 2; i++) {
        Vector2 p0 = footprint[0]; Vector2 p1 = footprint[i + 1]; Vector2 p2 = footprint[i + 2];
        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 down = { 0.0f, -1.0f, 0.0f };
        
        // Roof
        mesh.vertices[v*3+0] = p0.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p0.y;
        mesh.normals[v*3+0] = up.x; mesh.normals[v*3+1] = up.y; mesh.normals[v*3+2] = up.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = up.x; mesh.normals[v*3+1] = up.y; mesh.normals[v*3+2] = up.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = height; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = up.x; mesh.normals[v*3+1] = up.y; mesh.normals[v*3+2] = up.z; v++;
        
        // Floor
        mesh.vertices[v*3+0] = p0.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p0.y;
        mesh.normals[v*3+0] = down.x; mesh.normals[v*3+1] = down.y; mesh.normals[v*3+2] = down.z; v++;
        mesh.vertices[v*3+0] = p2.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p2.y;
        mesh.normals[v*3+0] = down.x; mesh.normals[v*3+1] = down.y; mesh.normals[v*3+2] = down.z; v++;
        mesh.vertices[v*3+0] = p1.x; mesh.vertices[v*3+1] = 0; mesh.vertices[v*3+2] = p1.y;
        mesh.normals[v*3+0] = down.x; mesh.normals[v*3+1] = down.y; mesh.normals[v*3+2] = down.z; v++;
    }

    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = color;
    return model;
}

// --- CORE MAP FUNCTIONS ---

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));

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
        else if (strncmp(line, "L ", 2) == 0) { 
             if (map.locationCount < MAX_LOCATIONS) {
                 int type; float x, y; char name[64];
                 if (sscanf(line, "L %d %f %f %63s", &type, &x, &y, name) == 4) {
                     map.locations[map.locationCount].position = (Vector2){ x * MAP_SCALE, y * MAP_SCALE };
                     map.locations[map.locationCount].type = (LocationType)type;
                     map.locations[map.locationCount].iconID = type;
                     for(int k=0; name[k]; k++) {
                         if(name[k] == '_') name[k] = ' ';
                     }
                     strncpy(map.locations[map.locationCount].name, name, 64);
                     map.locationCount++;
                 }
             }
        }
        else {
            if (mode == 1 && map.nodeCount < MAX_NODES) {
                int id; float x, y;
                if (sscanf(line, "%d: %f %f", &id, &x, &y) == 3) {
                    map.nodes[map.nodeCount].id = id;
                    map.nodes[map.nodeCount].position = (Vector2){x * MAP_SCALE, y * MAP_SCALE};
                    map.nodeCount++;
                }
            } else if (mode == 2 && map.edgeCount < MAX_EDGES) {
                int start, end; float width;
                if (sscanf(line, "%d %d %f", &start, &end, &width) == 3) {
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
                    if (pCount >= 3) {
                        build->model = GenerateBuildingMesh(build->footprint, build->pointCount, build->height, build->color);
                        map.buildingCount++;
                    }
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    UnloadFileText(text);
    return map;
}

void UnloadGameMap(GameMap *map) {
    if (map->nodes) free(map->nodes);
    if (map->edges) free(map->edges);
    for (int i = 0; i < map->buildingCount; i++) {
        UnloadModel(map->buildings[i].model);
        free(map->buildings[i].footprint);
    }
    if (map->buildings) free(map->buildings);
    if (map->locations) free(map->locations);
    if (map->graph) {
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }
}

void DrawGameMap(GameMap *map) {
    rlDisableBackfaceCulling(); 
    // Draw Roads
    for (int i = 0; i < map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;
        DrawRoadSegment((Vector3){s.x, 0, s.y}, (Vector3){en.x, 0, en.y}, e.width);
    }
    // Draw Buildings
    for (int i = 0; i < map->buildingCount; i++) {
        DrawModel(map->buildings[i].model, (Vector3){0,0,0}, 1.0f, WHITE);
        DrawModelWires(map->buildings[i].model, (Vector3){0,0,0}, 1.0f, BLACK); 
    }
    // NEW: Draw Locations (Red Spheres)
    for (int i = 0; i < map->locationCount; i++) {
        Vector3 pos = { map->locations[i].position.x, 1.0f, map->locations[i].position.y };
        DrawSphere(pos, 1.5f, Fade(RED, 0.8f));
        DrawSphereWires(pos, 1.55f, 8, 8, MAROON);
    }
    rlEnableBackfaceCulling();
}

bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    Vector2 p = { x, z };
    for (int i = 0; i < map->buildingCount; i++) {
        if (CheckCollisionPointPoly(p, map->buildings[i].footprint, map->buildings[i].pointCount)) {
            return true;
        }
    }
    return false;
}

// --- GRAPH & PATHFINDING ---

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

// <--- FIX: Robust Node Finder --->
// Skips isolated/orphan nodes to prevent "Graph Disconnected" errors
int GetClosestNode(GameMap *map, Vector2 position) {
    int bestNode = -1;
    float minDst = FLT_MAX;
    
    // Pass 1: Try to find a node WITH connections
    for (int i = 0; i < map->nodeCount; i++) {
        // Skip unconnected nodes
        if (map->graph && map->graph[i].count == 0) continue; 

        float d = Vector2DistanceSqr(position, map->nodes[i].position);
        if (d < minDst) {
            minDst = d;
            bestNode = i;
        }
    }
    
    // Pass 2: If we found nothing (unlikely), fall back to ANY node
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
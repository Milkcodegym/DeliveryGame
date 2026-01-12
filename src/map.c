#include "map.h"
#include "raymath.h"
#include "rlgl.h"
#include "player.h" 
#include "dealership.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

// --- CONFIGURATION ---
const float MAP_SCALE = 0.4f;

// [OPTIMIZATION] Drastically reduced render distance
const float RENDER_DIST_BASE = 100.0f;  
const float UNLOAD_DIST_BASE = 200.0f;
const float RENDER_DIST_SQUARED = 100.0f * 100.0f; // Pre-calculated for fast checks

typedef struct {
    int *buildingIndices;
    int buildingCount;
    int buildingCap;
    int *edgeIndices;
    int edgeCount;
    int edgeCap;
    int *areaIndices;
    int areaCount;
    int areaCap;
} SectorManifest;

// --- MAP BOUNDARIES (Dead Ends) ---
typedef struct {
    Vector3 position;
    Vector3 forward; // Direction pointing OUT of the map (the normal of the wall)
    float width;
    float angle;     // Rotation in degrees
    bool active;
} MapBoundary;

#define MAX_BOUNDARIES 128
static MapBoundary globalBoundaries[MAX_BOUNDARIES];
static int globalBoundaryCount = 0;

// Reset function
void ClearMapBoundaries() {
    globalBoundaryCount = 0;
    for(int i=0; i<MAX_BOUNDARIES; i++) globalBoundaries[i].active = false;
}

#define MODEL_SCALE 1.8f         
#define MODEL_Z_SQUISH 0.4f      
#define USE_INSTANCING 0         // [OPTIMIZATION] Disabled Instancing in favor of Baking
#define REGION_CENTER_RADIUS 600.0f 


// --- SPATIAL GRID CONFIG ---
// We divide the world into 100x100m chunks to speed up rendering
//set to 10 by 10 for testing
#define GRID_CELL_SIZE 100.0f
#define GRID_HASH_SIZE 1024 


// Sector Grid Configuration
#define GRID_CELL_SIZE 100.0f
#define SECTOR_GRID_ROWS 400
#define SECTOR_GRID_COLS 400
#define SECTOR_WORLD_OFFSET 20000.0f
#define MAX_ACTIVE_SECTORS 2048


// --- COLLISION OPTIMIZATION ---
typedef struct {
    int *indices;   // List of building indices in this cell
    int count;
    int capacity;
} CollisionCell;


typedef struct {
    int *indices;
    int count;
    int capacity;
} NodeCell;


static CollisionCell colGrid[SECTOR_GRID_ROWS][SECTOR_GRID_COLS] = {0};
static NodeCell nodeGrid[SECTOR_GRID_ROWS][SECTOR_GRID_COLS] = {0}; // [NEW]
static bool colGridLoaded = false;


typedef enum {
    // --- Existing Building Parts ---
    ASSET_AC_A = 0, ASSET_AC_B,
    ASSET_DOOR_BROWN, ASSET_DOOR_BROWN_GLASS, ASSET_DOOR_BROWN_WIN,
    ASSET_DOOR_WHITE, ASSET_DOOR_WHITE_GLASS, ASSET_DOOR_WHITE_WIN,
    ASSET_FRAME_DOOR1, ASSET_FRAME_SIMPLE, ASSET_FRAME_TENT,
    ASSET_FRAME_WIN, ASSET_FRAME_WIN_WHITE,
    ASSET_WIN_SIMPLE, ASSET_WIN_SIMPLE_W, ASSET_WIN_DET, ASSET_WIN_DET_W,
    ASSET_WIN_TWIN_TENT, ASSET_WIN_TWIN_TENT_W,
    ASSET_WIN_TALL, ASSET_WIN_TALL_TOP,
    ASSET_WALL, ASSET_CORNER, ASSET_SIDEWALK,
    
    // --- New Vegetation ---
    ASSET_PROP_TREE_LARGE, 
    ASSET_PROP_TREE_SMALL,
    ASSET_PROP_FLOWERS,
    ASSET_PROP_GRASS,


    // --- New Props ---
    ASSET_PROP_BENCH, 
    ASSET_PROP_TRASH,
    ASSET_PROP_LIGHT_CURVED, 
    ASSET_PROP_BOX,
    ASSET_PROP_CONE,
    ASSET_PROP_CONE_FLAT,
    ASSET_PROP_BARRIER,
    ASSET_PROP_CONST_LIGHT, 


    // --- Vehicles ---
    ASSET_CAR_DELIVERY,
    ASSET_CAR_HATCHBACK,
    ASSET_CAR_SEDAN,
    ASSET_CAR_SUV,
    ASSET_CAR_VAN,
    ASSET_CAR_POLICE,


    ASSET_COUNT
} AssetType;


typedef struct {
    AssetType window; AssetType windowTop; AssetType doorFrame; AssetType doorInner;
    AssetType balcony; bool hasAC; bool isSkyscraper; bool isWhiteTheme;
} BuildingStyle;


// [OPTIMIZATION] Sector Builder for Static Mesh Baking
typedef struct {
    float *vertices;   // 3 floats per vertex
    float *texcoords;  // 2 floats per vertex
    float *normals;    // 3 floats per vertex
    unsigned char *colors; // 4 bytes per vertex
    int vertexCount;
    int capacity;
} SectorBuilder;


typedef struct {
    Model model;
    Vector3 position;
    bool active;
    bool isEmpty;
    BoundingBox bounds;
    int activeListIndex;
    int loadStage; // 0=Inactive, 1=Basics, 2=Details, 3=Upload, 4=Done
} Sector;


typedef struct {
    int x;
    int y;
} SectorCoord;


typedef struct {
    Model models[ASSET_COUNT];        
    Sector sectors[SECTOR_GRID_ROWS][SECTOR_GRID_COLS];
    SectorBuilder *builders[SECTOR_GRID_ROWS][SECTOR_GRID_COLS];
    SectorManifest manifests[SECTOR_GRID_ROWS][SECTOR_GRID_COLS];
    SectorCoord activeSectors[MAX_ACTIVE_SECTORS];
    int activeSectorCount;

    int *nodeDegrees;
    bool loaded;
    Texture2D whiteTex;
    Texture2D sharedAtlas; 
    Model roadModel;
    Model markingsModel;
    Model areaModel;
    Model roofModel;
    bool mapBaked;

    Model signRoadClosed;
    Model signAccident;
    Model signConstruction;
    Model signLegModel;
    // [NEW] Staggered Loading State
    int loadingSectorX;
    int loadingSectorY;
    bool isSectorLoading;
} CityRenderSystem;


static CityRenderSystem cityRenderer = {0};
static SectorBuilder *currentActiveBuilder = NULL;

// [OPTIMIZATION] Persistent Memory Buffers
// We allocate these ONCE at startup and reuse them for every chunk load.
// This eliminates thousands of malloc/free calls per second (Lag Spike Fix).
static SectorBuilder globalSectorBuilder = {0}; 
static int globalTempIndices[4096]; // Pre-allocated buffer for roof triangulation

Color cityPalette[] = {
    {152, 251, 152, 255}, {255, 182, 193, 255}, {255, 105, 97, 255},  
    {255, 200, 150, 255}, {200, 200, 200, 255}  
};


// --- FORWARD DECLARATIONS ---
void BakeBuildingGeometry(Building *b);
void GenerateParkFoliage(GameMap *map, MapArea *area);
void BakeSingleEdgeDetails(GameMap *map, int edgeIdx);
void BakeObjectToSector(AssetType assetType, Vector3 pos, float rot, Vector3 scale, Color tint);
void InitSectorBuilder(SectorBuilder *sb);
void FreeSectorBuilder(SectorBuilder *sb);
Model BakeSectorMesh(SectorBuilder *sb);
void PushSectorTri(SectorBuilder *sb, Vector3 v1, Vector3 v2, Vector3 v3, Vector3 n1, Vector3 n2, Vector3 n3, Vector2 uv1, Vector2 uv2, Vector2 uv3, Color c);
void BakeSignWithLegs(Model signModel, Vector3 pos, float angleDeg);

// [FIX] Add these two new declarations so BakeSingleEdgeDetails can see them
float GetRaySegmentIntersection(Vector2 rayOrigin, Vector2 rayDir, Vector2 p1, Vector2 p2);
bool IsTooCloseToBuilding(GameMap *map, Vector2 pos, float minDistance);

// --- HELPER FUNCTIONS ---

// --- RENDER BOUNDARIES (Dynamic Mode) ---
void DrawMapBoundaries(Vector3 cameraPos) {
    // [OPTIMIZATION] Pre-calculate distance threshold squared (80m)
    // 80*80 = 6400. Anything further is skipped.
    float drawDistSqr = 6400.0f; 

    for (int i = 0; i < globalBoundaryCount; i++) {
        MapBoundary *b = &globalBoundaries[i];
        if (!b->active) continue;

        // [OPTIMIZATION] Fast Distance Check
        if (Vector3DistanceSqr(b->position, cameraPos) > drawDistSqr) continue;

        // 1. Draw Barriers
        Vector3 right = { -b->forward.z, 0, b->forward.x }; 
        int barrierCount = (int)(b->width / 2.5f) + 1;
        
        for (int k = 0; k < barrierCount; k++) {
            float t = (float)k / (barrierCount - 1);
            float offset = (t - 0.5f) * b->width;
            Vector3 pos = Vector3Add(b->position, Vector3Scale(right, offset));
            
            DrawModelEx(cityRenderer.models[ASSET_PROP_BARRIER], pos, (Vector3){0,1,0}, b->angle, (Vector3){2.5f, 2.5f, 2.5f}, WHITE);
        }

        // 2. Draw Sign
        Vector3 signPos = Vector3Add(b->position, Vector3Scale(b->forward, 1.5f));
        signPos.y = 1.8f;
        
        DrawModelEx(cityRenderer.signRoadClosed, signPos, (Vector3){0,1,0}, b->angle, (Vector3){1,1,1}, WHITE);
        
        // [OPTIMIZATION] Draw Legs using Cached Model instead of DrawCylinder
        float rad = b->angle * DEG2RAD;
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        float halfW = 0.8f;
        
        // Scale height to 1.8m
        Vector3 legScale = { 1.0f, 1.8f, 1.0f }; 

        Vector3 leg1 = { signPos.x - (cosA * halfW), 0.9f, signPos.z + (sinA * halfW) };
        Vector3 leg2 = { signPos.x + (cosA * halfW), 0.9f, signPos.z - (sinA * halfW) };
        
        DrawModelEx(cityRenderer.signLegModel, leg1, (Vector3){0,1,0}, b->angle, legScale, WHITE);
        DrawModelEx(cityRenderer.signLegModel, leg2, (Vector3){0,1,0}, b->angle, legScale, WHITE);
    }
}

// --- PHYSICS COLLISION ---
bool CheckBoundaryCollision(Vector3 pos, float radius) {
    for (int i = 0; i < globalBoundaryCount; i++) {
        MapBoundary *b = &globalBoundaries[i];
        
        // Simple Box Check
        // Project point relative to boundary center
        Vector3 relPos = Vector3Subtract(pos, b->position);
        
        // Rotate into local space (Un-rotate by b->angle)
        float rad = -b->angle * DEG2RAD;
        float localX = relPos.x * cosf(rad) - relPos.z * sinf(rad);
        float localZ = relPos.x * sinf(rad) + relPos.z * cosf(rad);
        
        float halfWidth = (b->width / 2.0f) + 1.0f; // Add buffer
        float thickness = 2.0f; // Thickness of the "invisible wall"

        if (fabs(localX) < halfWidth && fabs(localZ) < thickness) {
            return true; // Hit the wall
        }
    }
    return false;
}


Vector3 CalculateWallNormal(Vector2 p1, Vector2 p2) {
    Vector2 dir = Vector2Subtract(p2, p1);
    Vector2 normal2D = { -dir.y, dir.x };
    normal2D = Vector2Normalize(normal2D);
    return (Vector3){ normal2D.x, 0.0f, normal2D.y };
}


Vector2 GetBuildingCenter(Vector2 *footprint, int count) {
    Vector2 center = {0};
    if (count == 0) return center;
    for(int i = 0; i < count; i++) center = Vector2Add(center, footprint[i]);
    center.x /= count; center.y /= count;
    return center;
}


// --- TRIANGULATION HELPERS ---
float GetPolygonSignedArea(Vector2 *points, int count) {
    float area = 0.0f;
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        area += (points[j].x - points[i].x) * (points[j].y + points[i].y);
    }
    return area;
}


bool IsValidEar(Vector2 a, Vector2 b, Vector2 c, Vector2 *poly, int count, int *indices, int activeCount) {
    float cross = (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
    if (cross >= 0) return false; 


    for (int i = 0; i < activeCount; i++) {
        int idx = indices[i];
        Vector2 p = poly[idx];
        if (p.x == a.x && p.y == a.y) continue;
        if (p.x == b.x && p.y == b.y) continue;
        if (p.x == c.x && p.y == c.y) continue;
        
        if (CheckCollisionPointTriangle(p, a, b, c)) return false;
    }
    return true;
}


void BuildCollisionGrid(GameMap *map) {
    if (colGridLoaded) return;


    // Initialize Grid
    for(int y=0; y<SECTOR_GRID_ROWS; y++) {
        for(int x=0; x<SECTOR_GRID_COLS; x++) {
            colGrid[y][x].count = 0;
            colGrid[y][x].capacity = 0;
            colGrid[y][x].indices = NULL;
        }
    }


    // Populate Grid
    for (int i = 0; i < map->buildingCount; i++) {
        Vector2 center = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        
        int gx = (int)((center.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        int gy = (int)((center.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);


        // Safety Check
        if (gx < 0 || gx >= SECTOR_GRID_COLS || gy < 0 || gy >= SECTOR_GRID_ROWS) continue;


        // Add to cell
        CollisionCell *cell = &colGrid[gy][gx];
        if (cell->count >= cell->capacity) {
            cell->capacity = (cell->capacity == 0) ? 4 : cell->capacity * 2;
            cell->indices = (int*)realloc(cell->indices, cell->capacity * sizeof(int));
        }
        cell->indices[cell->count++] = i;
    }
    colGridLoaded = true;
    printf("Collision Grid Built.\n");
}


// --- MANIFEST SYSTEMS ---


void AddToManifest(SectorManifest *man, int index, int type) {
    // type 0: building, 1: edge, 2: area
    if (type == 0) {
        if (man->buildingCount >= man->buildingCap) {
            man->buildingCap = (man->buildingCap == 0) ? 8 : man->buildingCap * 2;
            man->buildingIndices = realloc(man->buildingIndices, man->buildingCap * sizeof(int));
        }
        man->buildingIndices[man->buildingCount++] = index;
    } else if (type == 1) {
        if (man->edgeCount >= man->edgeCap) {
            man->edgeCap = (man->edgeCap == 0) ? 8 : man->edgeCap * 2;
            man->edgeIndices = realloc(man->edgeIndices, man->edgeCap * sizeof(int));
        }
        man->edgeIndices[man->edgeCount++] = index;
    } else if (type == 2) {
        if (man->areaCount >= man->areaCap) {
            man->areaCap = (man->areaCap == 0) ? 8 : man->areaCap * 2;
            man->areaIndices = realloc(man->areaIndices, man->areaCap * sizeof(int));
        }
        man->areaIndices[man->areaCount++] = index;
    }
}


// Pre-calculates which sector every object belongs to
void BuildSectorManifests(GameMap *map) {
    // 1. Buildings
    for (int i = 0; i < map->buildingCount; i++) {
        Vector2 center = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        int gx = (int)((center.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        int gy = (int)((center.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        if (gx >= 0 && gx < SECTOR_GRID_COLS && gy >= 0 && gy < SECTOR_GRID_ROWS) {
            AddToManifest(&cityRenderer.manifests[gy][gx], i, 0);
        }
    }


    // 2. Edges (Roads) - Assign based on Midpoint
    for (int i = 0; i < map->edgeCount; i++) {
        if (map->edges[i].startNode >= map->nodeCount) continue;
        Vector2 p1 = map->nodes[map->edges[i].startNode].position;
        Vector2 p2 = map->nodes[map->edges[i].endNode].position;
        Vector2 mid = Vector2Scale(Vector2Add(p1, p2), 0.5f);
        
        int gx = (int)((mid.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        int gy = (int)((mid.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        
        // Also add to start/end sectors to prevent gaps? 
        // For simplicity, midpoint is usually enough for 100m chunks, 
        // but let's strictly check bounds.
        if (gx >= 0 && gx < SECTOR_GRID_COLS && gy >= 0 && gy < SECTOR_GRID_ROWS) {
            AddToManifest(&cityRenderer.manifests[gy][gx], i, 1);
        }
    }


    // 3. Areas (Parks)
    for (int i = 0; i < map->areaCount; i++) {
        Vector2 center = {0};
        for(int k=0; k<map->areas[i].pointCount; k++) center = Vector2Add(center, map->areas[i].points[k]);
        center = Vector2Scale(center, 1.0f/map->areas[i].pointCount);
        
        int gx = (int)((center.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        int gy = (int)((center.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        if (gx >= 0 && gx < SECTOR_GRID_COLS && gy >= 0 && gy < SECTOR_GRID_ROWS) {
            AddToManifest(&cityRenderer.manifests[gy][gx], i, 2);
        }
    }


    // 4. Pre-calc node degrees for road generation
    cityRenderer.nodeDegrees = (int*)calloc(map->nodeCount, sizeof(int));
    for(int i=0; i<map->edgeCount; i++) {
        if(map->edges[i].startNode < map->nodeCount) cityRenderer.nodeDegrees[map->edges[i].startNode]++;
        if(map->edges[i].endNode < map->nodeCount) cityRenderer.nodeDegrees[map->edges[i].endNode]++;
    }
}


int TriangulatePolygon(Vector2 *points, int count, int *outIndices) {
    if (count < 3) return 0;
    
    int *indices = (int*)malloc(count * sizeof(int));
    for(int i=0; i<count; i++) indices[i] = i;
    
    if (GetPolygonSignedArea(points, count) > 0) {
        for(int i=0; i<count/2; i++) {
            int temp = indices[i];
            indices[i] = indices[count-1-i];
            indices[count-1-i] = temp;
        }
    }


    int activeCount = count;
    int triCount = 0;
    int safeGuard = 0;


    while (activeCount > 2 && safeGuard < count * 3) {
        safeGuard++;
        bool earFound = false;
        
        for (int i = 0; i < activeCount; i++) {
            int prev = indices[(i - 1 + activeCount) % activeCount];
            int curr = indices[i];
            int next = indices[(i + 1) % activeCount];
            
            Vector2 a = points[prev];
            Vector2 b = points[curr];
            Vector2 c = points[next];
            float area = fabsf((b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x));
            
            if (area < 0.1f) { 
                for(int k=i; k<activeCount-1; k++) indices[k] = indices[k+1];
                activeCount--;
                earFound = true;
                break;
            }


            if (IsValidEar(a, b, c, points, count, indices, activeCount)) {
                outIndices[triCount*3+0] = prev;
                outIndices[triCount*3+1] = curr;
                outIndices[triCount*3+2] = next;
                triCount++;
                
                for(int k=i; k<activeCount-1; k++) indices[k] = indices[k+1];
                activeCount--;
                earFound = true;
                break;
            }
        }
        if (!earFound) break; 
    }
    
    free(indices);
    return triCount;
}


// --- BAKING SYSTEMS ---


void InitSectorBuilder(SectorBuilder *sb) {
    // Safety: If it already has memory, free it first to avoid leaks or corruption
    if (sb->vertices != NULL) FreeSectorBuilder(sb);

    sb->capacity = 4096; // Start with 4096 vertices
    sb->vertexCount = 0;
    
    // Allocate fresh memory
    sb->vertices = (float *)malloc(sb->capacity * 3 * sizeof(float));
    sb->texcoords = (float *)malloc(sb->capacity * 2 * sizeof(float));
    sb->normals = (float *)malloc(sb->capacity * 3 * sizeof(float));
    sb->colors = (unsigned char *)malloc(sb->capacity * 4 * sizeof(unsigned char));
    
    // Verify allocation
    if (!sb->vertices || !sb->texcoords || !sb->normals || !sb->colors) {
        printf("CRITICAL: Failed to allocate SectorBuilder memory!\n");
        sb->capacity = 0;
    }
}


void FreeSectorBuilder(SectorBuilder *sb) {
    if (sb->vertices) { free(sb->vertices); sb->vertices = NULL; }
    if (sb->texcoords) { free(sb->texcoords); sb->texcoords = NULL; }
    if (sb->normals) { free(sb->normals); sb->normals = NULL; }
    if (sb->colors) { free(sb->colors); sb->colors = NULL; }
    
    sb->vertexCount = 0;
    sb->capacity = 0;
}


void PushSectorTri(SectorBuilder *sb, 
                   Vector3 v1, Vector3 v2, Vector3 v3, 
                   Vector3 n1, Vector3 n2, Vector3 n3,
                   Vector2 uv1, Vector2 uv2, Vector2 uv3,
                   Color c) {
    
    // [FIX] Removed the "if (!sb->vertices) return;" check.
    // Instead, we check if we need to allocate memory (Capacity 0 or full).
    if (sb->vertices == NULL || sb->vertexCount + 3 >= sb->capacity) {
        int newCapacity = (sb->capacity == 0) ? 4096 : sb->capacity * 2;
        
        // Use realloc (handles NULL ptrs automatically by acting as malloc)
        float *newVerts = (float *)realloc(sb->vertices, newCapacity * 3 * sizeof(float));
        float *newTex = (float *)realloc(sb->texcoords, newCapacity * 2 * sizeof(float));
        float *newNorm = (float *)realloc(sb->normals, newCapacity * 3 * sizeof(float));
        unsigned char *newCols = (unsigned char *)realloc(sb->colors, newCapacity * 4 * sizeof(unsigned char));

        if (!newVerts || !newTex || !newNorm || !newCols) {
            printf("CRITICAL: Allocation failed in PushSectorTri!\n");
            return;
        }

        sb->vertices = newVerts;
        sb->texcoords = newTex;
        sb->normals = newNorm;
        sb->colors = newCols;
        sb->capacity = newCapacity;
    }

    int vc = sb->vertexCount;
    
    // Vertex 1
    sb->vertices[vc*3+0] = v1.x; sb->vertices[vc*3+1] = v1.y; sb->vertices[vc*3+2] = v1.z;
    sb->normals[vc*3+0] = n1.x; sb->normals[vc*3+1] = n1.y; sb->normals[vc*3+2] = n1.z;
    sb->texcoords[vc*2+0] = uv1.x; sb->texcoords[vc*2+1] = uv1.y;
    sb->colors[vc*4+0] = c.r; sb->colors[vc*4+1] = c.g; sb->colors[vc*4+2] = c.b; sb->colors[vc*4+3] = c.a;
    vc++;

    // Vertex 2
    sb->vertices[vc*3+0] = v2.x; sb->vertices[vc*3+1] = v2.y; sb->vertices[vc*3+2] = v2.z;
    sb->normals[vc*3+0] = n2.x; sb->normals[vc*3+1] = n2.y; sb->normals[vc*3+2] = n2.z;
    sb->texcoords[vc*2+0] = uv2.x; sb->texcoords[vc*2+1] = uv2.y;
    sb->colors[vc*4+0] = c.r; sb->colors[vc*4+1] = c.g; sb->colors[vc*4+2] = c.b; sb->colors[vc*4+3] = c.a;
    vc++;

    // Vertex 3
    sb->vertices[vc*3+0] = v3.x; sb->vertices[vc*3+1] = v3.y; sb->vertices[vc*3+2] = v3.z;
    sb->normals[vc*3+0] = n3.x; sb->normals[vc*3+1] = n3.y; sb->normals[vc*3+2] = n3.z;
    sb->texcoords[vc*2+0] = uv3.x; sb->texcoords[vc*2+1] = uv3.y;
    sb->colors[vc*4+0] = c.r; sb->colors[vc*4+1] = c.g; sb->colors[vc*4+2] = c.b; sb->colors[vc*4+3] = c.a;
    
    sb->vertexCount += 3;
}


// [OPTIMIZATION] Baking Function
// [FIX] Updated to handle Indexed Meshes (Fixes exploding/gap cubes)
void BakeModelToSector(SectorBuilder *sb, Model model, Vector3 pos, float rotDeg, Vector3 scale, Color tint) {
    if (model.meshCount == 0) return;
    
    // We only bake the first mesh for simple props/walls
    Mesh mesh = model.meshes[0]; 
    
    Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix matRot = MatrixRotateY(rotDeg * DEG2RAD);
    Matrix matTrans = MatrixTranslate(pos.x, pos.y, pos.z);
    Matrix transform = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);


    if (mesh.vertices == NULL) return; 


    // Calculate how many triangles we need to process
    int triCount = mesh.triangleCount;
    
    // Loop through every triangle
    for (int i = 0; i < triCount; i++) {
        int idx1, idx2, idx3;


        // [CRITICAL FIX] Check if mesh is indexed
        if (mesh.indices) {
            // If indexed, look up the vertex index from the index buffer
            idx1 = mesh.indices[i*3 + 0];
            idx2 = mesh.indices[i*3 + 1];
            idx3 = mesh.indices[i*3 + 2];
        } else {
            // If not indexed, just move linearly through the vertex buffer
            idx1 = i*3 + 0;
            idx2 = i*3 + 1;
            idx3 = i*3 + 2;
        }


        // Now extract the actual data using the correct indices
        Vector3 v1 = { mesh.vertices[idx1*3], mesh.vertices[idx1*3+1], mesh.vertices[idx1*3+2] };
        Vector3 v2 = { mesh.vertices[idx2*3], mesh.vertices[idx2*3+1], mesh.vertices[idx2*3+2] };
        Vector3 v3 = { mesh.vertices[idx3*3], mesh.vertices[idx3*3+1], mesh.vertices[idx3*3+2] };


        Vector3 n1 = { 0, 1, 0 }; Vector3 n2 = { 0, 1, 0 }; Vector3 n3 = { 0, 1, 0 };
        if (mesh.normals) {
             n1 = (Vector3){ mesh.normals[idx1*3], mesh.normals[idx1*3+1], mesh.normals[idx1*3+2] };
             n2 = (Vector3){ mesh.normals[idx2*3], mesh.normals[idx2*3+1], mesh.normals[idx2*3+2] };
             n3 = (Vector3){ mesh.normals[idx3*3], mesh.normals[idx3*3+1], mesh.normals[idx3*3+2] };
        }


        Vector2 uv1 = { 0, 0 }; Vector2 uv2 = { 0, 0 }; Vector2 uv3 = { 0, 0 };
        if (mesh.texcoords) {
            uv1 = (Vector2){ mesh.texcoords[idx1*2], mesh.texcoords[idx1*2+1] };
            uv2 = (Vector2){ mesh.texcoords[idx2*2], mesh.texcoords[idx2*2+1] };
            uv3 = (Vector2){ mesh.texcoords[idx3*2], mesh.texcoords[idx3*2+1] };
        }


        // Transform Geometry
        v1 = Vector3Transform(v1, transform);
        v2 = Vector3Transform(v2, transform);
        v3 = Vector3Transform(v3, transform);


        // Simple normal rotation
        n1 = Vector3Transform(n1, matRot);
        n2 = Vector3Transform(n2, matRot);
        n3 = Vector3Transform(n3, matRot);


        PushSectorTri(sb, v1, v2, v3, n1, n2, n3, uv1, uv2, uv3, tint);
    }
}


// Updated Mesh Generation to include UVs
Model BakeMeshFromBuffers(float *vertices, float *colors, float *uvs, int triangleCount) {
    Mesh mesh = { 0 };
    mesh.triangleCount = triangleCount;
    mesh.vertexCount = triangleCount * 3;
    mesh.vertices = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = (unsigned char *)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    mesh.normals = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(mesh.vertexCount * 2 * sizeof(float));


    if (vertices) memcpy(mesh.vertices, vertices, mesh.vertexCount * 3 * sizeof(float));
    
    if (colors) {
        // Assume colors input is char* from SectorBuilder but current legacy calls pass float*
        // We will adapt inside the builder-based baking, but legacy calls need this check.
        // Actually, let's keep this function signature for Legacy calls (Roads), 
        // and add a new one for SectorBuilder.
    }
    
    // Default fallback (filled by caller usually)
    for (int i = 0; i < mesh.vertexCount; i++) {
        mesh.normals[i*3 + 0] = 0.0f; mesh.normals[i*3 + 1] = 1.0f; mesh.normals[i*3 + 2] = 0.0f;
    }
    
    if (uvs) {
        memcpy(mesh.texcoords, uvs, mesh.vertexCount * 2 * sizeof(float));
    } else {
         for (int i = 0; i < mesh.vertexCount * 2; i++) mesh.texcoords[i] = 0.0f;
    }


    // Color Handling - if float* passed (legacy)
    if (colors) {
         // Legacy path for Road/Area/Roof bakes which pass float* colors
         // We handle this manually in the calling functions or here? 
         // Let's refactor the calls instead to use a cleaner approach?
         // No, simpler to just handle the loop:
         // Wait, the prompt implies "Update BakeMeshFromBuffers".
    }


    return LoadModelFromMesh(mesh);
}


// Dedicated function to bake from SectorBuilder
Model BakeSectorMesh(SectorBuilder *sb) {
    Mesh mesh = { 0 };
    mesh.triangleCount = sb->vertexCount / 3;
    mesh.vertexCount = sb->vertexCount;
    
    mesh.vertices = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    mesh.colors = (unsigned char *)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));


    memcpy(mesh.vertices, sb->vertices, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.normals, sb->normals, mesh.vertexCount * 3 * sizeof(float));
    memcpy(mesh.texcoords, sb->texcoords, mesh.vertexCount * 2 * sizeof(float));
    memcpy(mesh.colors, sb->colors, mesh.vertexCount * 4 * sizeof(unsigned char));


    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}


SectorBuilder* GetSectorBuilderForPos(Vector3 pos) {
    int x = (int)((pos.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int y = (int)((pos.z + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    
    if (x >= 0 && x < SECTOR_GRID_COLS && y >= 0 && y < SECTOR_GRID_ROWS) {
        if (cityRenderer.builders[y][x] == NULL) {
            cityRenderer.builders[y][x] = (SectorBuilder*)malloc(sizeof(SectorBuilder));
            InitSectorBuilder(cityRenderer.builders[y][x]);
        }
        return cityRenderer.builders[y][x];
    }
    return NULL;
}


// Replaces ADD_INSTANCE
void BakeObjectToSector(AssetType assetType, Vector3 pos, float rot, Vector3 scale, Color tint) {
    // MUST use currentActiveBuilder, ignoring the object's global position
    if (currentActiveBuilder) {
        BakeModelToSector(currentActiveBuilder, cityRenderer.models[assetType], pos, rot, scale, tint);
    }
}


// [FIXED] Better RNG and reduced density to prevent "Tree Explosion"
unsigned int GetSpatialHash(Vector3 pos) {
    unsigned int x = (unsigned int)((int)pos.x * 73856093);
    unsigned int z = (unsigned int)((int)pos.z * 19349663);
    return x ^ z;
}


// --- NEW HELPER: Generates a rounded cap at a node to fix gaps/make curves ---
void BakeRoadNodeCap(Vector2 center, float radius, Color color) {
    if (currentActiveBuilder == NULL) return;

    int segments = 12; // Level of detail for the curve
    float step = 360.0f / segments;
    
    Vector3 c = { center.x, 0.15f, center.y }; // Same height as road
    Vector3 up = { 0, 1, 0 };

    for (int i = 0; i < segments; i++) {
        float angle1 = (i * step) * DEG2RAD;
        float angle2 = ((i + 1) * step) * DEG2RAD;

        Vector3 p1 = { center.x + sinf(angle1) * radius, 0.15f, center.y + cosf(angle1) * radius };
        Vector3 p2 = { center.x + sinf(angle2) * radius, 0.15f, center.y + cosf(angle2) * radius };

        // Push triangle fan
        PushSectorTri(currentActiveBuilder, c, p2, p1, up, up, up, (Vector2){0,0}, (Vector2){0,0}, (Vector2){0,0}, color);
    }
}

// --- NEW HELPER: Checks if a point is inside the asphalt of nearby roads ---
// --- UPDATED HELPER: Checks if a point is inside the asphalt of nearby roads ---
// Added a safety buffer to push props further away from the road edge
bool IsPointOnAsphalt(GameMap *map, Vector2 pos, int currentNode) {
    if (map->graph == NULL || currentNode >= map->nodeCount) return false;

    NodeGraph *nodeData = &map->graph[currentNode];
    
    // Check all roads connected to this intersection
    for (int i = 0; i < nodeData->count; i++) {
        int edgeIdx = nodeData->connections[i].edgeIndex;
        Edge e = map->edges[edgeIdx];
        
        Vector2 start = map->nodes[e.startNode].position;
        Vector2 end = map->nodes[e.endNode].position;
        
        // [FIX] Added +0.8f buffer. 
        // Logic: "If point is within (RoadWidth + Buffer), it is on asphalt."
        float unsafeRadius = (e.width * MAP_SCALE) + 0.8f; 

        // Project point onto line segment
        Vector2 pa = Vector2Subtract(pos, start);
        Vector2 ba = Vector2Subtract(end, start);
        float h = Clamp(Vector2DotProduct(pa, ba) / Vector2DotProduct(ba, ba), 0.0f, 1.0f);
        Vector2 closest = Vector2Add(start, Vector2Scale(ba, h));
        
        if (Vector2Distance(pos, closest) < unsafeRadius) {
            return true;
        }
    }
    return false;
}

void BakeSingleEdgeDetails(GameMap *map, int edgeIdx) {
    Edge e = map->edges[edgeIdx];
    float sidewalkW = 2.5f; 
    float lightSpacing = 16.0f; 
    
    Vector2 s = map->nodes[e.startNode].position;
    Vector2 en = map->nodes[e.endNode].position;
    
    float roadHalfW = e.width * MAP_SCALE; 
    float finalRoadW = roadHalfW * 2.0f;
    
    Vector2 dir = Vector2Normalize(Vector2Subtract(en, s));
    Vector2 right = { -dir.y, dir.x };
    float len = Vector2Distance(s, en);
    float angle = atan2f(dir.y, dir.x) * RAD2DEG; 

    float offsetDist = roadHalfW + (sidewalkW / 2.0f);
    
    if (!cityRenderer.nodeDegrees) return;

    // Corner cutting logic
    float cutFactor = finalRoadW * 0.1f; 
    float startCut = (cityRenderer.nodeDegrees[e.startNode] > 2) ? cutFactor : 0.0f;
    float endCut = (cityRenderer.nodeDegrees[e.endNode] > 2) ? cutFactor : 0.0f;
    
    if (startCut + endCut > len * 0.9f) {
        float factor = (len * 0.9f) / (startCut + endCut);
        startCut *= factor; endCut *= factor;
    }

    // 1. Bake Road Geometry
    {
        Vector3 start = {s.x, 0.15f, s.y}; 
        Vector3 end = {en.x, 0.15f, en.y};
        Vector3 halfW = Vector3Scale((Vector3){right.x, 0, right.y}, finalRoadW*0.5f);
        
        float overlapAmount = 0.5f; 
        Vector3 extension = Vector3Scale((Vector3){dir.x, 0, dir.y}, overlapAmount);

        Vector3 drawStart = Vector3Subtract(start, extension);
        Vector3 drawEnd   = Vector3Add(end, extension);

        Vector3 v1 = Vector3Subtract(drawStart, halfW);
        Vector3 v2 = Vector3Add(drawStart, halfW);
        Vector3 v3 = Vector3Add(drawEnd, halfW);
        Vector3 v4 = Vector3Subtract(drawEnd, halfW);
        
        PushSectorTri(currentActiveBuilder, v1, v2, v3, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector2){0,0}, (Vector2){0,0}, (Vector2){0,0}, COLOR_ROAD);
        PushSectorTri(currentActiveBuilder, v1, v3, v4, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector2){0,0}, (Vector2){0,0}, (Vector2){0,0}, COLOR_ROAD);

        BakeRoadNodeCap(s, roadHalfW, COLOR_ROAD);
        BakeRoadNodeCap(en, roadHalfW, COLOR_ROAD);
    }

    // --- 2. MAP BOUNDARY LOGIC (Dead Ends) ---
    // Instead of baking meshes (which loses textures), we register a logic boundary.
    
    // Check Start Node
    if (cityRenderer.nodeDegrees[e.startNode] == 1) {
        if (globalBoundaryCount < MAX_BOUNDARIES) {
            MapBoundary *b = &globalBoundaries[globalBoundaryCount++];
            // Position slightly into the road
            Vector2 pos2D = Vector2Add(s, Vector2Scale(dir, 2.0f)); 
            b->position = (Vector3){ pos2D.x, 0.0f, pos2D.y };
            b->width = finalRoadW;
            b->angle = -angle + 90.0f; // Perpendicular to road
            b->forward = (Vector3){ dir.x, 0, dir.y }; // Pointing towards the road (normal)
            b->active = true;
        }
    }

    // Check End Node
    if (cityRenderer.nodeDegrees[e.endNode] == 1) {
        if (globalBoundaryCount < MAX_BOUNDARIES) {
            MapBoundary *b = &globalBoundaries[globalBoundaryCount++];
            Vector2 pos2D = Vector2Subtract(en, Vector2Scale(dir, 2.0f)); 
            b->position = (Vector3){ pos2D.x, 0.0f, pos2D.y };
            b->width = finalRoadW;
            b->angle = -angle - 90.0f;
            b->forward = (Vector3){ -dir.x, 0, -dir.y }; // Pointing towards the road
            b->active = true;
        }
    }

    // 3. Sidewalks & Props (Standard Loop)
    Color sidewalkTint = (Color){180, 180, 180, 255};
    Color treeTint = (Color){40, 110, 40, 255};
    Color benchTint = (Color){100, 70, 40, 255};
    Color trashTint = (Color){50, 50, 60, 255};
    Color lightTint = WHITE; 

    for(int side=-1; side<=1; side+=2) {
        Vector2 sideOffset = Vector2Scale(right, offsetDist * side);
        Vector2 rawStart = Vector2Add(s, sideOffset);
        Vector2 swStart = Vector2Add(rawStart, Vector2Scale(dir, startCut));
        float swLen = len - (startCut + endCut);
        
        if (swLen < 0.1f) continue;

        Vector2 swMid = Vector2Add(swStart, Vector2Scale(dir, swLen/2.0f));
        Vector3 swPos = { swMid.x, 0.07f, swMid.y }; 
        Vector3 swScale = { swLen, 0.10f, sidewalkW };
        BakeObjectToSector(ASSET_SIDEWALK, swPos, -angle, swScale, sidewalkTint);

        float currentDist = 0.0f;
        float nextLight = lightSpacing * 0.5f; 

        while (currentDist < swLen) {
            Vector2 propPos2D = Vector2Add(swStart, Vector2Scale(dir, currentDist));
            Vector3 propPos = { propPos2D.x, 0.2f, propPos2D.y };
            
            int checkNode = (currentDist < swLen / 2.0f) ? e.startNode : e.endNode;
            
            bool safeToSpawn = true;
            if (IsTooCloseToBuilding(map, propPos2D, 1.5f)) safeToSpawn = false;
            if (safeToSpawn && IsPointOnAsphalt(map, propPos2D, checkNode)) safeToSpawn = false;

            if (safeToSpawn) {
                if (currentDist >= nextLight) {
                    float lightRot = -angle - 90.0f; 
                    if (side == 1) lightRot += 90.0f; else lightRot -= 90.0f; 
                    BakeObjectToSector(ASSET_PROP_LIGHT_CURVED, propPos, lightRot, (Vector3){2.8f, 2.8f, 2.8f}, lightTint);
                    nextLight += lightSpacing;
                } else {
                    unsigned int seed = GetSpatialHash(propPos);
                    int roll = seed % 100; 
                    float baseRot = (side == 1) ? -angle : -angle + 180.0f;

                    if (roll < 2) BakeObjectToSector(ASSET_PROP_TRASH, propPos, baseRot, (Vector3){1.2f,1.2f,1.2f}, trashTint);
                    else if (roll < 5) BakeObjectToSector(ASSET_PROP_BENCH, propPos, baseRot, (Vector3){1.5f,1.5f,1.5f}, benchTint);
                    else if (roll < 12) BakeObjectToSector(ASSET_PROP_TREE_SMALL, propPos, (float)(seed%360), (Vector3){4.5f,4.5f,4.5f}, treeTint);
                }
            }
            currentDist += 3.5f;
        }
    }
}

// Tracks where we left off inside a specific list (e.g., building #12)
static int globalLoadIterator = 0;

// Returns TRUE if there is more work to do (Keep calling me!)
// Returns FALSE if the sector is fully loaded
bool ProcessSectorLoadStep(GameMap *map) {
    if (!cityRenderer.isSectorLoading) return false;

    int x = cityRenderer.loadingSectorX;
    int y = cityRenderer.loadingSectorY;
    Sector *sec = &cityRenderer.sectors[y][x];
    SectorBuilder *sb = &globalSectorBuilder;
    currentActiveBuilder = sb; 

    SectorManifest *man = &cityRenderer.manifests[y][x];

    // --- STAGE 0: SETUP ---
    if (sec->loadStage == 0) {
        if (sb->capacity == 0) InitSectorBuilder(sb);
        sb->vertexCount = 0; 
        sec->loadStage = 1;
        globalLoadIterator = 0; // Reset iterator for Stage 1
        return true; 
    }

    // --- STAGE 1: BUILDINGS (Micro-Tasked) ---
    if (sec->loadStage == 1) {
        int batchSize = 5; // Only bake 5 buildings per frame
        int processed = 0;

        while (processed < batchSize && globalLoadIterator < man->buildingCount) {
            BakeBuildingGeometry(&map->buildings[man->buildingIndices[globalLoadIterator]]);
            globalLoadIterator++;
            processed++;
        }

        // If we finished all buildings, move to next stage
        if (globalLoadIterator >= man->buildingCount) {
            sec->loadStage = 2;
            globalLoadIterator = 0; // Reset for Stage 2
        }
        return true; 
    }

    // --- STAGE 2: ROADS (Micro-Tasked) ---
    if (sec->loadStage == 2) {
        int batchSize = 10; // Bake 10 roads per frame (roads are cheaper than buildings)
        int processed = 0;

        while (processed < batchSize && globalLoadIterator < man->edgeCount) {
            BakeSingleEdgeDetails(map, man->edgeIndices[globalLoadIterator]);
            globalLoadIterator++;
            processed++;
        }

        if (globalLoadIterator >= man->edgeCount) {
            sec->loadStage = 3;
            globalLoadIterator = 0; 
        }
        return true;
    }

    // --- STAGE 3: ROOF TRIANGULATION (Micro-Tasked) ---
    if (sec->loadStage == 3) {
        int batchSize = 5; 
        int processed = 0;
        int *tempIndices = globalTempIndices; 

        while (processed < batchSize && globalLoadIterator < man->buildingCount) {
            Building *b = &map->buildings[man->buildingIndices[globalLoadIterator]];
            if (b->pointCount >= 3) {
                float yPos = b->height + 0.1f; 
                Color roofCol = {80, 80, 90, 255};
                int triCount = TriangulatePolygon(b->footprint, b->pointCount, tempIndices);
                for (int k = 0; k < triCount; k++) {
                    int idx1 = tempIndices[k*3+0];
                    int idx2 = tempIndices[k*3+1];
                    int idx3 = tempIndices[k*3+2];
                    Vector3 v1 = {b->footprint[idx1].x, yPos, b->footprint[idx1].y};
                    Vector3 v2 = {b->footprint[idx2].x, yPos, b->footprint[idx2].y};
                    Vector3 v3 = {b->footprint[idx3].x, yPos, b->footprint[idx3].y};
                    PushSectorTri(sb, v1, v2, v3, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector2){0,0}, (Vector2){0,0}, (Vector2){0,0}, roofCol);
                }
            }
            globalLoadIterator++;
            processed++;
        }

        if (globalLoadIterator >= man->buildingCount) {
            sec->loadStage = 4;
            globalLoadIterator = 0;
        }
        return true;
    }

    // --- STAGE 4: PARKS (All at once - usually few per chunk) ---
    if (sec->loadStage == 4) {
        for (int i = 0; i < man->areaCount; i++) {
            MapArea *area = &map->areas[man->areaIndices[i]];
            if (area->color.g > area->color.r) GenerateParkFoliage(map, area); 
            if (area->pointCount >= 3) {
                Vector2 center = {0};
                for(int k=0; k<area->pointCount; k++) center = Vector2Add(center, area->points[k]);
                center = Vector2Scale(center, 1.0f/area->pointCount);
                Color c = (area->color.g > area->color.r) ? COLOR_PARK : area->color;
                for(int j=0; j<area->pointCount; j++) {
                    Vector3 v1 = {center.x, 0.02f, center.y}; 
                    Vector3 v2 = {area->points[(j+1)%area->pointCount].x, 0.02f, area->points[(j+1)%area->pointCount].y};
                    Vector3 v3 = {area->points[j].x, 0.02f, area->points[j].y}; 
                    PushSectorTri(sb, v1, v2, v3, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector3){0,1,0}, (Vector2){0,0}, (Vector2){0,0}, (Vector2){0,0}, c);
                }
            }
        }
        sec->loadStage = 5;
        return true;
    }

    // --- STAGE 5: GPU UPLOAD ---
    if (sec->loadStage == 5) {
        if (sb->vertexCount > 0) {
            sec->model = BakeSectorMesh(sb);
            if (cityRenderer.whiteTex.id != 0) {
                for(int m=0; m<sec->model.meshCount; m++) {
                    sec->model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex;
                }
            }
            sec->isEmpty = false;
        } else {
            sec->isEmpty = true;
        }
        
        sec->active = true;
        sec->activeListIndex = cityRenderer.activeSectorCount;
        cityRenderer.activeSectors[cityRenderer.activeSectorCount].x = x;
        cityRenderer.activeSectors[cityRenderer.activeSectorCount].y = y;
        cityRenderer.activeSectorCount++;
        
        sec->loadStage = 6; 
        cityRenderer.isSectorLoading = false; 
        currentActiveBuilder = NULL;
        return false; 
    }

    return false;
}

// [FIX] Helper to prevent double-freeing shared textures
void UnloadModelSafe(Model model) {
    // 1. Basic Validity Checks
    if (model.meshCount == 0 || model.materialCount == 0) return;
    
    // [CRITICAL FIX] If the pointers are NULL (or likely invalid), stop immediately.
    // This catches scenarios where a model might be partially zeroed or corrupt.
    if (model.materials == NULL || model.meshes == NULL) return;


    // 2. Detach Texture (Safe now because we checked model.materials is not NULL)
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){0}; 
    }
    
    // 3. Unload
    UnloadModel(model);
}


void UnloadSectorChunk(int x, int y) {
    Sector *sec = &cityRenderer.sectors[y][x];
    if (!sec->active) return;
    
    // 1. Unload Model Memory
    if (!sec->isEmpty) {
        UnloadModelSafe(sec->model);
    }
    sec->active = false;
    sec->isEmpty = false;


    // 2. [OPTIMIZATION] Remove from Active List (Swap with last)
    int idx = sec->activeListIndex;
    int lastIdx = cityRenderer.activeSectorCount - 1;
    
    if (idx != lastIdx && lastIdx >= 0) {
        // Move the last element into the empty slot
        cityRenderer.activeSectors[idx] = cityRenderer.activeSectors[lastIdx];
        
        // Update the moved sector's index reference
        int movedX = cityRenderer.activeSectors[idx].x;
        int movedY = cityRenderer.activeSectors[idx].y;
        cityRenderer.sectors[movedY][movedX].activeListIndex = idx;
    }
    
    cityRenderer.activeSectorCount--;
    // Inside UnloadGameMap (at the very end of the renderer unloading block)
    if (globalSectorBuilder.capacity > 0) {
    FreeSectorBuilder(&globalSectorBuilder);
    // No need to free(&globalSectorBuilder) because it's static
}
}


void UpdateMapStreaming(GameMap *map, Vector3 playerPos) {
    if (!cityRenderer.loaded) return;

    // 1. [PRIORITY] If we are in the middle of loading a chunk, work on it.
    // We do NOT scan for new chunks until this one is done.
    if (cityRenderer.isSectorLoading) {
        ProcessSectorLoadStep(map);
        return; 
    }

    int px = (int)((playerPos.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int py = (int)((playerPos.z + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);

    int loadRadius = (int)(RENDER_DIST_BASE / GRID_CELL_SIZE); 
    if (loadRadius < 1) loadRadius = 1;
    int unloadRadius = loadRadius + 2; 

    // 2. Unload Far Sectors (This is fast, can do all at once)
    for (int i = cityRenderer.activeSectorCount - 1; i >= 0; i--) {
        int sx = cityRenderer.activeSectors[i].x;
        int sy = cityRenderer.activeSectors[i].y;
        
        int dx = abs(sx - px);
        int dy = abs(sy - py);
        
        if (dx > unloadRadius || dy > unloadRadius) {
            UnloadSectorChunk(sx, sy);
            // Reset stage so it can be reloaded later if needed
            cityRenderer.sectors[sy][sx].loadStage = 0; 
        }
    }

    // 3. Find ONE new sector to start loading
    // We scan outward from player
    for (int y = py - loadRadius; y <= py + loadRadius; y++) {
        for (int x = px - loadRadius; x <= px + loadRadius; x++) {
            if (x >= 0 && x < SECTOR_GRID_COLS && y >= 0 && y < SECTOR_GRID_ROWS) {
                if (!cityRenderer.sectors[y][x].active && !cityRenderer.isSectorLoading) {
                    
                    // Found a candidate. Start the process.
                    cityRenderer.loadingSectorX = x;
                    cityRenderer.loadingSectorY = y;
                    cityRenderer.isSectorLoading = true;
                    
                    // Initialize stage 0 immediately
                    cityRenderer.sectors[y][x].loadStage = 0;
                    
                    // Process the first step (Setup) immediately so we don't waste a frame
                    ProcessSectorLoadStep(map);
                    return; // Only start ONE chunk per frame cycle
                }
            }
        }
    }
}

// Legacy function for road baking (adapted to new signature/logic if needed, but kept separate for safety)
Model BakeStaticGeometryLegacy(float *vertices, float *colors, int triangleCount) {
    Mesh mesh = { 0 };
    mesh.triangleCount = triangleCount;
    mesh.vertexCount = triangleCount * 3;
    mesh.vertices = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = (unsigned char *)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    mesh.normals = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(mesh.vertexCount * 2 * sizeof(float));


    memcpy(mesh.vertices, vertices, mesh.vertexCount * 3 * sizeof(float));
    
    if (colors) {
        for (int i = 0; i < mesh.vertexCount; i++) {
            mesh.colors[i*4 + 0] = (unsigned char)(colors[i*4 + 0] * 255);
            mesh.colors[i*4 + 1] = (unsigned char)(colors[i*4 + 1] * 255);
            mesh.colors[i*4 + 2] = (unsigned char)(colors[i*4 + 2] * 255);
            mesh.colors[i*4 + 3] = (unsigned char)(colors[i*4 + 3] * 255);
        }
    } else {
        for (int i = 0; i < mesh.vertexCount; i++) {
            mesh.colors[i*4] = 255; mesh.colors[i*4+1] = 255; mesh.colors[i*4+2] = 255; mesh.colors[i*4+3] = 255;
        }
    }
    // Default normals/UVs for roads
    for (int i = 0; i < mesh.vertexCount; i++) {
        mesh.normals[i*3 + 0] = 0.0f; mesh.normals[i*3 + 1] = 1.0f; mesh.normals[i*3 + 2] = 0.0f;
        mesh.texcoords[i*2] = 0.0f; mesh.texcoords[i*2+1] = 0.0f;
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}


// Helper: Check if a point is reasonably close to the city (prevents void spawning)
bool IsInsideCityContext(GameMap *map, Vector2 pos) {
    // 1. Simple Bounding Box Check (Fast)
    // You should cache these values in the map struct ideally, but calculating them here is okay for now
    static float minX = 10000.0f, maxX = -10000.0f, minY = 10000.0f, maxY = -10000.0f;
    static bool boundsCalculated = false;


    if (!boundsCalculated && map->buildingCount > 0) {
        for(int i=0; i<map->buildingCount; i++) {
             if (map->buildings[i].footprint[0].x < minX) minX = map->buildings[i].footprint[0].x;
             if (map->buildings[i].footprint[0].x > maxX) maxX = map->buildings[i].footprint[0].x;
             if (map->buildings[i].footprint[0].y < minY) minY = map->buildings[i].footprint[0].y;
             if (map->buildings[i].footprint[0].y > maxY) maxY = map->buildings[i].footprint[0].y;
        }
        // Add buffer
        minX -= 20.0f; maxX += 20.0f; minY -= 20.0f; maxY += 20.0f;
        boundsCalculated = true;
    }


    if (pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY) return false;


    // 2. Proximity Check (Slower but accurate)
    // Must be within 60 meters of a building to be considered "City"
    for (int i=0; i<map->buildingCount; i++) {
         // Fast dist check to first point of building
         if (Vector2DistanceSqr(pos, map->buildings[i].footprint[0]) < 60.0f*60.0f) return true;
    }


    return false;
}




void GenerateParkFoliage(GameMap *map, MapArea *area) {
    if (area->pointCount < 3) return;
    
    float minX = FLT_MAX, maxX = -FLT_MAX, minY = FLT_MAX, maxY = -FLT_MAX;
    for(int i=0; i<area->pointCount; i++) {
        if(area->points[i].x < minX) minX = area->points[i].x;
        if(area->points[i].x > maxX) maxX = area->points[i].x;
        if(area->points[i].y < minY) minY = area->points[i].y;
        if(area->points[i].y > maxY) maxY = area->points[i].y;
    }
    
    float areaW = maxX - minX;
    float areaH = maxY - minY;
    int itemPoints = (int)((areaW * areaH) / 60.0f);
    if(itemPoints > 80) itemPoints = 80; 
    
    Color treeTint = (Color){20, 90, 40, 255};
    Color grassTint = (Color){60, 110, 20, 255};
    Color flowerTint = (Color){200, 200, 200, 255};


    for(int k=0; k<itemPoints; k++) {
        float tx = GetRandomValue((int)minX, (int)maxX);
        float ty = GetRandomValue((int)minY, (int)maxY);
        Vector2 tPos = {tx, ty};
        
        if (CheckCollisionPointPoly(tPos, area->points, area->pointCount)) {
            Vector3 pos = {tPos.x, 0.0f, tPos.y};
            float rot = GetRandomValue(0, 360);
            
            int typeRoll = GetRandomValue(0, 100);
            
            if (typeRoll < 20) {
                // Large Tree (5x Scale)
                Vector3 scale = { 7.5f, 7.5f, 7.5f };
                BakeObjectToSector(ASSET_PROP_TREE_LARGE, pos, rot, scale, treeTint);
            } else if (typeRoll < 50) {
                // Small Tree (5x Scale)
                Vector3 scale = { 5.0f, 5.0f, 5.0f };
                BakeObjectToSector(ASSET_PROP_TREE_SMALL, pos, rot, scale, treeTint);
            } else if (typeRoll < 80) {
                Vector3 scale = { 2.0f, 1.0f, 2.0f };
                BakeObjectToSector(ASSET_PROP_GRASS, pos, rot, scale, grassTint);
            } else {
                Vector3 scale = { 1.5f, 1.0f, 1.5f };
                BakeObjectToSector(ASSET_PROP_FLOWERS, pos, rot, scale, flowerTint);
            }
        }
    }
}


// --- HELPER FOR GENERATION ---
// Checks if a grid cell is close to a line segment (Roads)
bool IsPointNearSegment(Vector2 p, Vector2 a, Vector2 b, float threshold) {
    Vector2 pa = Vector2Subtract(p, a);
    Vector2 ba = Vector2Subtract(b, a);
    float lenSq = Vector2DotProduct(ba, ba);
    
    // Fix: Prevent division by zero if startNode == endNode
    if (lenSq < 0.0001f) return (Vector2DistanceSqr(p, a) < threshold * threshold);


    float h = Clamp(Vector2DotProduct(pa, ba) / lenSq, 0.0f, 1.0f);
    Vector2 distVec = Vector2Subtract(pa, Vector2Scale(ba, h));
    return (Vector2LengthSqr(distVec) < threshold * threshold);
}


// Math helper: Intersection of Ray (Origin, Dir) vs Segment (P1, P2)
// Returns distance, or FLT_MAX if no hit
float GetRaySegmentIntersection(Vector2 rayOrigin, Vector2 rayDir, Vector2 p1, Vector2 p2) {
    Vector2 v1 = Vector2Subtract(rayOrigin, p1);
    Vector2 v2 = Vector2Subtract(p2, p1);
    Vector2 v3 = {-rayDir.y, rayDir.x};


    float dot = Vector2DotProduct(v2, v3);
    if (fabs(dot) < 0.000001f) return FLT_MAX; // Parallel


    float t1 = Vector2CrossProduct(v2, v1) / dot;
    float t2 = Vector2DotProduct(v1, v3) / dot;


    if (t1 >= 0.0f && (t2 >= 0.0f && t2 <= 1.0f)) {
        return t1;
    }
    return FLT_MAX;
}


// Find nearest obstacle distance in a specific direction
float CastParkRay(GameMap *map, Vector2 origin, Vector2 dir, float maxDist) {
    float closest = maxDist;


    // 1. Check Buildings
    // Using the sector grid for speed
    int gx = (int)((origin.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int gy = (int)((origin.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);


    for (int y = gy - 1; y <= gy + 1; y++) {
        for (int x = gx - 1; x <= gx + 1; x++) {
            if (x < 0 || x >= SECTOR_GRID_COLS || y < 0 || y >= SECTOR_GRID_ROWS) continue;
            
            CollisionCell *cell = &colGrid[y][x];
            for (int k = 0; k < cell->count; k++) {
                Building *b = &map->buildings[cell->indices[k]];
                for (int p = 0; p < b->pointCount; p++) {
                    Vector2 w1 = b->footprint[p];
                    Vector2 w2 = b->footprint[(p+1) % b->pointCount];
                    float d = GetRaySegmentIntersection(origin, dir, w1, w2);
                    if (d < closest) closest = d;
                }
            }
        }
    }


    // 2. Check Roads
    // Critical: Park must stop BEFORE the road curb.
    for (int i = 0; i < map->edgeCount; i++) {
        if (map->edges[i].startNode >= map->nodeCount || map->edges[i].endNode >= map->nodeCount) continue;
        
        Vector2 n1 = map->nodes[map->edges[i].startNode].position;
        Vector2 n2 = map->nodes[map->edges[i].endNode].position;


        // Bounding box optimization
        if (origin.x < fminf(n1.x, n2.x) - maxDist || origin.x > fmaxf(n1.x, n2.x) + maxDist) continue;
        if (origin.y < fminf(n1.y, n2.y) - maxDist || origin.y > fmaxf(n1.y, n2.y) + maxDist) continue;


        float distToCenter = GetRaySegmentIntersection(origin, dir, n1, n2);
        
        if (distToCenter < closest) {
            // Subtract Road Half Width + Sidewalk Width (approx 5.0m + 2.5m = ~7.5m)
            // If we are closer than that, we are INSIDE the road -> invalid park
            float safeDistance = (map->edges[i].width * MAP_SCALE) + 3.5f; 
            
            if (distToCenter < safeDistance) {
                return 0.0f; // Invalid, we are inside road/sidewalk
            }
            closest = distToCenter - safeDistance;
        }
    }


    return closest;
}


void UpdateRuntimeParks(GameMap *map, Vector3 playerPos) {
    int cx = (int)((playerPos.x + PARK_OFFSET) / PARK_CHUNK_SIZE);
    int cy = (int)((playerPos.z + PARK_OFFSET) / PARK_CHUNK_SIZE);


    for (int y = cy - 1; y <= cy + 1; y++) {
        for (int x = cx - 1; x <= cx + 1; x++) {
            
            if (x < 0 || x >= PARK_GRID_COLS || y < 0 || y >= PARK_GRID_ROWS) continue;
            if (parkSystem.chunks[y][x].generated) continue;


            parkSystem.chunks[y][x].generated = true;
            parkSystem.chunks[y][x].parkCount = 0;


            float chunkWorldX = (x * PARK_CHUNK_SIZE) - PARK_OFFSET;
            float chunkWorldY = (y * PARK_CHUNK_SIZE) - PARK_OFFSET;


            int attempts = 15;
            for (int k = 0; k < attempts; k++) {
                if (parkSystem.totalParks >= MAX_DYNAMIC_PARKS) break;
                if (parkSystem.chunks[y][x].parkCount >= PARK_MAX_PER_CHUNK) break;


                Vector2 seed = {
                    chunkWorldX + GetRandomValue(5, PARK_CHUNK_SIZE - 5),
                    chunkWorldY + GetRandomValue(5, PARK_CHUNK_SIZE - 5)
                };


                // [FIX] VOID CHECK: Don't generate if we aren't near buildings
                if (!IsInsideCityContext(map, seed)) continue;


                // [FIX] COLLISION CHECK: Ensure seed isn't already inside something
                if (CastParkRay(map, seed, (Vector2){1,0}, 2.0f) < 0.5f) continue;


                DynamicPark park;
                park.center = seed;
                park.active = true;
                bool validPark = true;
                float minRayLen = FLT_MAX;
                float maxRayLen = 0.0f;


                for (int r = 0; r < PARK_RAYS; r++) {
                    float angle = (r / (float)PARK_RAYS) * 360.0f * DEG2RAD;
                    Vector2 dir = { cosf(angle), sinf(angle) };
                    
                    float dist = CastParkRay(map, seed, dir, 35.0f); // 35m Max Radius
                    
                    // If Ray is tiny, we are pinched against a wall
                    if (dist < 0.2f) { validPark = false; break; }
                    
                    if (dist < minRayLen) minRayLen = dist;
                    if (dist > maxRayLen) maxRayLen = dist;
                    
                    park.vertices[r] = Vector2Add(seed, Vector2Scale(dir, dist));
                }


                // Only allow parks that have decent open space (at least 3m radius in smallest dir)
                if (validPark && minRayLen > 0.5f) {
                    int pIdx = parkSystem.totalParks++;
                    parkSystem.parks[pIdx] = park;
                    parkSystem.chunks[y][x].parkIndices[parkSystem.chunks[y][x].parkCount++] = pIdx;
                }
            }
        }
    }
}


void DrawRuntimeParks(Vector3 playerPos) {
    int cx = (int)((playerPos.x + PARK_OFFSET) / PARK_CHUNK_SIZE);
    int cy = (int)((playerPos.z + PARK_OFFSET) / PARK_CHUNK_SIZE);
    
    int viewDist = 3; 


    // Enable double-sided drawing so we see the floor regardless of winding order
    rlDisableBackfaceCulling();


    for (int y = cy - viewDist; y <= cy + viewDist; y++) {
        for (int x = cx - viewDist; x <= cx + viewDist; x++) {
            if (x < 0 || x >= PARK_GRID_COLS || y < 0 || y >= PARK_GRID_ROWS) continue;
            
            ParkChunk *chunk = &parkSystem.chunks[y][x];
            if (!chunk->generated) continue;


            for (int i = 0; i < chunk->parkCount; i++) {
                DynamicPark *p = &parkSystem.parks[chunk->parkIndices[i]];
                if (!p->active) continue;


                // 1. Draw Green Floor (Manual 3D Fan)
                // We draw individual triangles connecting Center -> Vertex A -> Vertex B
                Vector3 center = {p->center.x, 0.04f, p->center.y};
                Color parkColor = (Color){30, 90, 40, 255};


                for (int v = 0; v < PARK_RAYS; v++) {
                    int next = (v + 1) % PARK_RAYS;
                    
                    Vector3 v1 = {p->vertices[v].x, 0.04f, p->vertices[v].y};
                    Vector3 v2 = {p->vertices[next].x, 0.04f, p->vertices[next].y};
                    
                    // Draw the wedge
                    DrawTriangle3D(center, v1, v2, parkColor);
                    // Draw duplicate flipped to ensure visibility if culling is weird
                    DrawTriangle3D(center, v2, v1, parkColor);
                }
                
                // 2. Draw Trees (Deterministic)
                // Use seed based on position
                int seed = (int)(p->center.x * 100) + (int)(p->center.y * 100);
                SetRandomSeed(seed);
                
                float approxRadius = Vector2Distance(p->center, p->vertices[0]);
                int treeCount = (int)(approxRadius / 2.0f);
                if (treeCount < 1) treeCount = 1;
                if (treeCount > 8) treeCount = 8;


                for(int k=0; k<treeCount; k++) {
                    int vIdx = GetRandomValue(0, PARK_RAYS-1);
                    float lerp = GetRandomValue(20, 70) / 100.0f;
                    
                    Vector2 pos2D = Vector2Lerp(p->center, p->vertices[vIdx], lerp);
                    Vector3 pos = { pos2D.x, 0.0f, pos2D.y };
                    
                    if (GetRandomValue(0,10) > 3) {
                         DrawModelEx(cityRenderer.models[ASSET_PROP_TREE_SMALL], pos, (Vector3){0,1,0}, GetRandomValue(0,360), (Vector3){5.0f,5.0f,5.0f}, (Color){40,110,40,255});
                    } else {
                         DrawModelEx(cityRenderer.models[ASSET_PROP_GRASS], pos, (Vector3){0,1,0}, GetRandomValue(0,360), (Vector3){1.5f,1.0f,1.5f}, (Color){60,110,20,255});
                    }
                }
            }
        }
    }
    rlEnableBackfaceCulling();
}


void BakeMapElements(GameMap *map) {
    if (cityRenderer.mapBaked) return;
    printf("Baking Map Geometry...\n");


    int maxRoadTris = map->edgeCount * 2 + 5000;
    int maxMarkTris = map->edgeCount * 2 + 5000;
    if (maxRoadTris > 200000) maxRoadTris = 200000; 


    float *roadVerts = (float*)malloc(maxRoadTris * 3 * 3 * sizeof(float));
    float *markVerts = (float*)malloc(maxMarkTris * 3 * 3 * sizeof(float));
    int rVCount = 0;
    int mVCount = 0;


    for (int i = 0; i < map->edgeCount; i++) {
        if (rVCount >= maxRoadTris * 3 - 18) break; 


        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;
        float finalWidth = (e.width * MAP_SCALE) * 2.0f;
        Vector3 start = {s.x, 0.15f, s.y}; 
        Vector3 end = {en.x, 0.15f, en.y};
        Vector3 diff = Vector3Subtract(end, start);
        if (Vector3Length(diff) < 0.001f) continue;


        Vector3 right = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, diff));
        Vector3 halfWidthVec = Vector3Scale(right, finalWidth * 0.5f);
        
        Vector3 v1 = Vector3Subtract(start, halfWidthVec); 
        Vector3 v2 = Vector3Add(start, halfWidthVec);      
        Vector3 v3 = Vector3Add(end, halfWidthVec);        
        Vector3 v4 = Vector3Subtract(end, halfWidthVec);   
        
        roadVerts[rVCount++] = v1.x; roadVerts[rVCount++] = v1.y; roadVerts[rVCount++] = v1.z;
        roadVerts[rVCount++] = v3.x; roadVerts[rVCount++] = v3.y; roadVerts[rVCount++] = v3.z;
        roadVerts[rVCount++] = v2.x; roadVerts[rVCount++] = v2.y; roadVerts[rVCount++] = v2.z;
        roadVerts[rVCount++] = v1.x; roadVerts[rVCount++] = v1.y; roadVerts[rVCount++] = v1.z;
        roadVerts[rVCount++] = v4.x; roadVerts[rVCount++] = v4.y; roadVerts[rVCount++] = v4.z;
        roadVerts[rVCount++] = v3.x; roadVerts[rVCount++] = v3.y; roadVerts[rVCount++] = v3.z;


        if (!e.oneway) {
            float lineWidth = finalWidth * 0.05f; 
            Vector3 lineOffset = Vector3Scale(right, lineWidth * 0.5f);
            Vector3 mStart = { start.x, 0.165f, start.z }; 
            Vector3 mEnd = { end.x, 0.165f, end.z };
            Vector3 l1 = Vector3Subtract(mStart, lineOffset);
            Vector3 l2 = Vector3Add(mStart, lineOffset);
            Vector3 l3 = Vector3Add(mEnd, lineOffset);
            Vector3 l4 = Vector3Subtract(mEnd, lineOffset);
            
            markVerts[mVCount++] = l1.x; markVerts[mVCount++] = l1.y; markVerts[mVCount++] = l1.z;
            markVerts[mVCount++] = l3.x; markVerts[mVCount++] = l3.y; markVerts[mVCount++] = l3.z;
            markVerts[mVCount++] = l2.x; markVerts[mVCount++] = l2.y; markVerts[mVCount++] = l2.z;
            markVerts[mVCount++] = l1.x; markVerts[mVCount++] = l1.y; markVerts[mVCount++] = l1.z;
            markVerts[mVCount++] = l4.x; markVerts[mVCount++] = l4.y; markVerts[mVCount++] = l4.z;
            markVerts[mVCount++] = l3.x; markVerts[mVCount++] = l3.y; markVerts[mVCount++] = l3.z;
        }
    }


    cityRenderer.roadModel = BakeStaticGeometryLegacy(roadVerts, NULL, rVCount / 3);
    
    for(int i=0; i<cityRenderer.roadModel.meshCount; i++) {
        for(int j=0; j<cityRenderer.roadModel.meshes[i].vertexCount*4; j+=4) {
            cityRenderer.roadModel.meshes[i].colors[j] = COLOR_ROAD.r;
            cityRenderer.roadModel.meshes[i].colors[j+1] = COLOR_ROAD.g;
            cityRenderer.roadModel.meshes[i].colors[j+2] = COLOR_ROAD.b;
            cityRenderer.roadModel.meshes[i].colors[j+3] = COLOR_ROAD.a;
        }
    }
    UploadMesh(&cityRenderer.roadModel.meshes[0], false);


    cityRenderer.markingsModel = BakeStaticGeometryLegacy(markVerts, NULL, mVCount / 3);
    Color mk = COLOR_ROAD_MARKING;
    for(int i=0; i<cityRenderer.markingsModel.meshCount; i++) {
        for(int j=0; j<cityRenderer.markingsModel.meshes[i].vertexCount; j++) {
            cityRenderer.markingsModel.meshes[i].colors[j*4+0] = mk.r;
            cityRenderer.markingsModel.meshes[i].colors[j*4+1] = mk.g;
            cityRenderer.markingsModel.meshes[i].colors[j*4+2] = mk.b;
            cityRenderer.markingsModel.meshes[i].colors[j*4+3] = mk.a;
        }
    }
    UploadMesh(&cityRenderer.markingsModel.meshes[0], false);


    free(roadVerts);
    free(markVerts);


    int maxAreaVerts = map->areaCount * 50 * 3; 
    float *areaVerts = (float*)malloc(maxAreaVerts * 3 * sizeof(float));
    float *areaColors = (float*)malloc(maxAreaVerts * 4 * sizeof(float));
    int aVCount = 0;


    for (int i = 0; i < map->areaCount; i++) {
        if (aVCount >= maxAreaVerts - 50) break; 


        if(map->areas[i].pointCount < 3) continue;
        Color col = map->areas[i].color;
        if (col.g > col.r && col.g > col.b) {
            col = COLOR_PARK; 
            GenerateParkFoliage(map, &map->areas[i]);
        }


        float r = col.r/255.0f; float g = col.g/255.0f; float b = col.b/255.0f;
        Vector3 center = {0, 0.01f, 0};
        for(int j=0; j<map->areas[i].pointCount; j++) { center.x += map->areas[i].points[j].x; center.z += map->areas[i].points[j].y; }
        center.x /= map->areas[i].pointCount; center.z /= map->areas[i].pointCount;


        for(int j=0; j<map->areas[i].pointCount; j++) {
            Vector2 p1 = map->areas[i].points[j]; 
            Vector2 p2 = map->areas[i].points[(j+1)%map->areas[i].pointCount];
            areaVerts[aVCount*3+0] = center.x; areaVerts[aVCount*3+1] = 0.01f; areaVerts[aVCount*3+2] = center.z;
            areaColors[aVCount*4+0] = r; areaColors[aVCount*4+1] = g; areaColors[aVCount*4+2] = b; areaColors[aVCount*4+3] = 1.0f;
            aVCount++;
            areaVerts[aVCount*3+0] = p2.x; areaVerts[aVCount*3+1] = 0.01f; areaVerts[aVCount*3+2] = p2.y;
            areaColors[aVCount*4+0] = r; areaColors[aVCount*4+1] = g; areaColors[aVCount*4+2] = b; areaColors[aVCount*4+3] = 1.0f;
            aVCount++;
            areaVerts[aVCount*3+0] = p1.x; areaVerts[aVCount*3+1] = 0.01f; areaVerts[aVCount*3+2] = p1.y;
            areaColors[aVCount*4+0] = r; areaColors[aVCount*4+1] = g; areaColors[aVCount*4+2] = b; areaColors[aVCount*4+3] = 1.0f;
            aVCount++;
        }
    }
    cityRenderer.areaModel = BakeStaticGeometryLegacy(areaVerts, areaColors, aVCount / 3);
    free(areaVerts);
    free(areaColors);


    int maxRoofVerts = map->buildingCount * 12 * 3; 
    float *roofVerts = (float*)malloc(maxRoofVerts * 3 * sizeof(float));
    float *roofColors = (float*)malloc(maxRoofVerts * 4 * sizeof(float)); 
    int rfVCount = 0;


    int *tempIndices = (int*)malloc(500 * sizeof(int)); 


    for (int i = 0; i < map->buildingCount; i++) {
        if (rfVCount >= maxRoofVerts - 50) break; 


        Building *b = &map->buildings[i];
        if (b->pointCount < 3) continue;
        
        float y = b->height + 0.1f; 
        float rr = 80.0f/255.0f; float rg = 80.0f/255.0f; float rb = 90.0f/255.0f; 


        int triCount = TriangulatePolygon(b->footprint, b->pointCount, tempIndices);
        
        for (int k = 0; k < triCount; k++) {
            int idx1 = tempIndices[k*3+0];
            int idx2 = tempIndices[k*3+1];
            int idx3 = tempIndices[k*3+2];
            
            Vector2 p1 = b->footprint[idx1];
            Vector2 p2 = b->footprint[idx2];
            Vector2 p3 = b->footprint[idx3];


            roofVerts[rfVCount*3+0] = p1.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = p1.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
            
            roofVerts[rfVCount*3+0] = p2.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = p2.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
            
            roofVerts[rfVCount*3+0] = p3.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = p3.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
        }
    }
    free(tempIndices);
    
    cityRenderer.roofModel = BakeStaticGeometryLegacy(roofVerts, roofColors, rfVCount / 3);
    free(roofVerts);
    free(roofColors);


    cityRenderer.mapBaked = true;
    printf("Baking Done. V-Roads:%d, V-Marks:%d, V-Areas:%d, V-Roofs:%d\n", rVCount/3, mVCount/3, aVCount/3, rfVCount/3);
}


// --- NEW HELPER FUNCTION ---
// Collapses all UV coordinates of a mesh to a single point.
// Use this to map procedural objects to a "white pixel" on your atlas.
void SetMeshUVs(Mesh *mesh, float u, float v) {
    if (mesh->texcoords == NULL) return;
    for (int i = 0; i < mesh->vertexCount; i++) {
        mesh->texcoords[i*2] = u;
        mesh->texcoords[i*2+1] = v;
    }
}

// --- NEW HELPER: Generates a 3D Sign Model with custom text ---
Model GenerateSignModel(const char* text, Color bgCol, Color textCol) {
    // 1. Draw Text to Texture
    int texW = 256;
    int texH = 128;
    RenderTexture2D target = LoadRenderTexture(texW, texH);
    
    BeginTextureMode(target);
        ClearBackground(bgCol);
        // Draw heavy borders
        DrawRectangleLines(0, 0, texW, texH, textCol); 
        DrawRectangleLines(4, 4, texW-8, texH-8, textCol);
        
        // Center Text
        int fontSize = 30; // Fits "ROAD CLOSED" well
        int textWidth = MeasureText(text, fontSize);
        DrawText(text, texW/2 - textWidth/2, texH/2 - fontSize/2, fontSize, textCol);
    EndTextureMode();

    // 2. Convert to Texture2D (Must flip vertical due to OpenGL coords)
    Image img = LoadImageFromTexture(target.texture);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    UnloadRenderTexture(target);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    // 3. Create Board Mesh
    // 2.0m wide, 1.0m tall, thin
    Mesh mesh = GenMeshCube(2.0f, 1.0f, 0.15f);
    
    // 4. Create Model
    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    
    return model;
}

void LoadCityAssets() {
    if (cityRenderer.loaded) return;
    
    // --- 1. Load the Atlas ---
    if (FileExists("resources/Buildings/Textures/colormap.png")) {
        cityRenderer.whiteTex = LoadTexture("resources/Buildings/Textures/colormap.png");
        SetTextureFilter(cityRenderer.whiteTex, TEXTURE_FILTER_BILINEAR); 
    } else {
        Image whiteImg = GenImageColor(1, 1, WHITE);
        cityRenderer.whiteTex = LoadTextureFromImage(whiteImg);
        UnloadImage(whiteImg);
    }


    // Helper macro
    #define LOAD_ASSET(enumIdx, path) \
        { \
            cityRenderer.models[enumIdx] = LoadModel(path); \
            if (cityRenderer.models[enumIdx].meshCount == 0) { \
                printf("Failed to load: %s\n", path); \
                cityRenderer.models[enumIdx] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); \
            } \
        }


    // --- Buildings ---
    LOAD_ASSET(ASSET_AC_A, "resources/Buildings/detail-ac-a.obj");
    LOAD_ASSET(ASSET_AC_B, "resources/Buildings/detail-ac-b.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN, "resources/Buildings/door-brown.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_GLASS, "resources/Buildings/door-brown-glass.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_WIN, "resources/Buildings/door-brown-window.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE, "resources/Buildings/door-white.obj"); 
    LOAD_ASSET(ASSET_DOOR_WHITE_GLASS, "resources/Buildings/door-white-glass.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE_WIN, "resources/Buildings/door-white-window.obj");
    LOAD_ASSET(ASSET_FRAME_DOOR1, "resources/Buildings/door1.obj");
    LOAD_ASSET(ASSET_FRAME_SIMPLE, "resources/Buildings/simple_door.obj");
    LOAD_ASSET(ASSET_FRAME_TENT, "resources/Buildings/doorframe_glass_tent.obj");
    LOAD_ASSET(ASSET_FRAME_WIN, "resources/Buildings/window_door.obj");
    LOAD_ASSET(ASSET_FRAME_WIN_WHITE, "resources/Buildings/window_door_white.obj");
    LOAD_ASSET(ASSET_WIN_SIMPLE, "resources/Buildings/Windows_simple.obj");
    LOAD_ASSET(ASSET_WIN_SIMPLE_W, "resources/Buildings/Windows_simple_white.obj");
    LOAD_ASSET(ASSET_WIN_DET, "resources/Buildings/Windows_detailed.obj");
    LOAD_ASSET(ASSET_WIN_DET_W, "resources/Buildings/Windows_detailed_white.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT, "resources/Buildings/Twin_window_tents.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT_W, "resources/Buildings/Twin_window_tents_white.obj");
    LOAD_ASSET(ASSET_WIN_TALL, "resources/Buildings/windows_tall.obj");
    LOAD_ASSET(ASSET_WIN_TALL_TOP, "resources/Buildings/windows_tall_top.obj");


    // --- Props & Vegetation ---
    LOAD_ASSET(ASSET_PROP_TREE_LARGE, "resources/trees/tree-large.obj");
    LOAD_ASSET(ASSET_PROP_TREE_SMALL, "resources/trees/tree-small.obj");
    LOAD_ASSET(ASSET_PROP_BENCH, "resources/random/bench.obj");
    LOAD_ASSET(ASSET_PROP_FLOWERS, "resources/random/flowers.obj");
    LOAD_ASSET(ASSET_PROP_GRASS, "resources/random/grass.obj");
    LOAD_ASSET(ASSET_PROP_TRASH, "resources/random/trash.obj");
    
    LOAD_ASSET(ASSET_PROP_BOX, "resources/Props/box.obj");
    LOAD_ASSET(ASSET_PROP_CONE, "resources/Props/cone.obj");
    LOAD_ASSET(ASSET_PROP_CONE_FLAT, "resources/Props/cone-flat.obj");
    LOAD_ASSET(ASSET_PROP_BARRIER, "resources/Props/construction-barrier.obj");
    LOAD_ASSET(ASSET_PROP_CONST_LIGHT, "resources/Props/construction-light.obj");
    LOAD_ASSET(ASSET_PROP_LIGHT_CURVED, "resources/Props/light-curved.obj");


    // --- Vehicles ---
    LOAD_ASSET(ASSET_CAR_DELIVERY, "resources/Playermodels/delivery.obj");
    LOAD_ASSET(ASSET_CAR_HATCHBACK, "resources/Playermodels/hatchback-sport.obj");
    LOAD_ASSET(ASSET_CAR_SEDAN, "resources/Playermodels/sedan.obj");
    LOAD_ASSET(ASSET_CAR_SUV, "resources/Playermodels/suv.obj");
    LOAD_ASSET(ASSET_CAR_VAN, "resources/Playermodels/van.obj");
    LOAD_ASSET(ASSET_CAR_POLICE, "resources/Playermodels/police.obj");


    // --- Fix UVs for Procedural/Color-Tinted Objects ---
    float atlasW = 512.0f; 
    float atlasH = 512.0f;
    float whitePixelU = (200.0f + 0.5f) / atlasW;
    float whitePixelV = (400.0f + 0.5f) / atlasH;

    //road signs
    cityRenderer.signRoadClosed = GenerateSignModel("ROAD CLOSED", ORANGE, BLACK);
    cityRenderer.signConstruction = GenerateSignModel("WORK ZONE", ORANGE, BLACK);
    // Red/White for Accident
    cityRenderer.signAccident = GenerateSignModel("ACCIDENT", RED, WHITE);
    // [NEW] Generate a shared leg model (Simple gray box)
    Mesh legMesh = GenMeshCube(0.1f, 1.0f, 0.1f); // Unit size
    
    // [FIX] Allocate memory for colors! GenMeshCube returns NULL for colors.
    legMesh.colors = (unsigned char *)MemAlloc(legMesh.vertexCount * 4 * sizeof(unsigned char));

    // Paint it dark gray
    for(int i = 0; i < legMesh.vertexCount * 4; i += 4) {
        legMesh.colors[i] = 50;     // R
        legMesh.colors[i+1] = 50;   // G
        legMesh.colors[i+2] = 50;   // B
        legMesh.colors[i+3] = 255;  // A
    }

    // LoadModelFromMesh uploads the mesh (including our new colors) to the GPU automatically
    cityRenderer.signLegModel = LoadModelFromMesh(legMesh);

    // 1. Procedural Cubes
    Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    SetMeshUVs(&cubeMesh, whitePixelU, whitePixelV);
    Model cubeModel = LoadModelFromMesh(cubeMesh);
    Material propMat = LoadMaterialDefault();
    propMat.maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex;
    cubeModel.materials[0] = propMat;


    cityRenderer.models[ASSET_WALL] = cubeModel;
    cityRenderer.models[ASSET_CORNER] = cubeModel;
    cityRenderer.models[ASSET_SIDEWALK] = cubeModel;


    // 2. Apply Atlas Fix to Props so we can Tint them (Green trees, etc)
    // We iterate the specific props that need simple coloring
    AssetType tintableProps[] = { 
        ASSET_PROP_TREE_LARGE, ASSET_PROP_TREE_SMALL, ASSET_PROP_GRASS, 
        ASSET_PROP_FLOWERS, ASSET_PROP_BENCH, ASSET_PROP_TRASH, 
        ASSET_PROP_LIGHT_CURVED, ASSET_PROP_CONE, ASSET_PROP_CONE_FLAT 
    };
    
    for (int i=0; i < 9; i++) {
        AssetType t = tintableProps[i];
        if (cityRenderer.models[t].meshCount > 0) {
            SetMeshUVs(&cityRenderer.models[t].meshes[0], whitePixelU, whitePixelV);
            cityRenderer.models[t].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex;
        }
    }


    // Initialize Sector Builders
    for (int y = 0; y < SECTOR_GRID_ROWS; y++) {
        for (int x = 0; x < SECTOR_GRID_COLS; x++) {
            cityRenderer.builders[y][x] = NULL;
            cityRenderer.sectors[y][x].active = false;
        }
    }
    // [NEW] Initialize the Global Builder once
    if (globalSectorBuilder.capacity == 0) {
        InitSectorBuilder(&globalSectorBuilder);
    }

    cityRenderer.loaded = true;
}


// Spawns a sign board + 2 legs
void BakeSignWithLegs(Model signModel, Vector3 pos, float angleDeg) {
    if (!currentActiveBuilder) return;

    // 1. Bake the Board (Floating)
    // Board center is at 'pos'. 
    Vector3 boardScale = { 1.0f, 1.0f, 1.0f }; // Model is already 2x1m
    BakeModelToSector(currentActiveBuilder, signModel, pos, angleDeg, boardScale, WHITE);

    // 2. Bake Legs (Using Generic Wall/Cube)
    // Calculate leg positions relative to center
    float halfWidth = 0.8f; 
    float legHeight = pos.y; // From ground to center of sign
    Vector3 legScale = { 0.1f, legHeight + 0.5f, 0.1f }; // +0.5 to go into the board
    
    float rad = angleDeg * DEG2RAD;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    // Left Leg
    Vector3 lPos = { 
        pos.x - (cosA * halfWidth), 
        legHeight / 2.0f, 
        pos.z + (sinA * halfWidth) 
    };
    
    // Right Leg
    Vector3 rPos = { 
        pos.x + (cosA * halfWidth), 
        legHeight / 2.0f, 
        pos.z - (sinA * halfWidth) 
    };

    // Use dark gray for legs
    Color legColor = { 50, 50, 50, 255 };
    BakeObjectToSector(ASSET_WALL, lPos, angleDeg, legScale, legColor);
    BakeObjectToSector(ASSET_WALL, rPos, angleDeg, legScale, legColor);
}

BuildingStyle GetBuildingStyle(Vector2 pos) {
    BuildingStyle style = {0};
    float distToCenter = Vector2Length(pos);
    bool isCenter = (distToCenter < REGION_CENTER_RADIUS);
    int roll = GetRandomValue(0, 100);
    if (isCenter && roll < 60) {
        style.isSkyscraper = true; style.window = ASSET_WIN_TALL; style.windowTop = ASSET_WIN_TALL_TOP;
        style.doorFrame = ASSET_FRAME_DOOR1; style.doorInner = ASSET_DOOR_BROWN_GLASS;
        style.hasAC = false; style.isWhiteTheme = false;
    } else if (roll < 30) {
        style.isSkyscraper = false; style.isWhiteTheme = false; style.window = ASSET_WIN_DET;
        style.windowTop = ASSET_WIN_DET; style.doorFrame = ASSET_FRAME_TENT; style.doorInner = ASSET_DOOR_BROWN;
        style.hasAC = true;
    } else if (roll < 70) {
        style.isSkyscraper = false; style.isWhiteTheme = true; style.window = ASSET_WIN_TWIN_TENT_W;
        style.windowTop = ASSET_WIN_TWIN_TENT_W; style.doorFrame = ASSET_FRAME_WIN_WHITE;
        style.doorInner = ASSET_DOOR_WHITE_WIN; style.hasAC = true;
    } else {
        style.isSkyscraper = false; style.isWhiteTheme = false; style.window = ASSET_WIN_SIMPLE;
        style.windowTop = ASSET_WIN_SIMPLE; style.doorFrame = ASSET_FRAME_SIMPLE; style.doorInner = ASSET_DOOR_BROWN_WIN;
        style.hasAC = true;
    }
    return style;
}


void BakeBuildingGeometry(Building *b) {
    float floorHeight = 3.0f * (MODEL_SCALE / 4.0f); 
    Vector2 bCenter = GetBuildingCenter(b->footprint, b->pointCount);
    BuildingStyle style = GetBuildingStyle(bCenter);
    
    if (style.isSkyscraper) floorHeight *= 0.85f; 
    int rawFloors = (int)(b->height / floorHeight);
    if (style.isSkyscraper) { if (rawFloors < 6) rawFloors = 6; } 
    else { if (rawFloors < 2) rawFloors = 2; if (rawFloors > 5) rawFloors = 5; }
    
    int floors = rawFloors;
    float visualHeight = floors * floorHeight;
    b->height = visualHeight; 
    
    int colorIdx = (abs((int)b->footprint[0].x) + abs((int)b->footprint[0].y)) % 5;
    if (style.isWhiteTheme) colorIdx = 5; 
    Color tint = (colorIdx == 5) ? WHITE : cityPalette[colorIdx];


    float structuralDepth = MODEL_SCALE * MODEL_Z_SQUISH; 
    float cornerThick = structuralDepth * 0.85f; 
    
    // [FIX] Shrink Factor: Pull walls inward to prevent clipping
    float shrinkAmount = 0.3f; 


    for (int i = 0; i < b->pointCount; i++) {
        // Get raw points
        Vector2 rawP1 = b->footprint[i];
        Vector2 rawP2 = b->footprint[(i + 1) % b->pointCount];


        // [FIX] Calculate Inset Points
        // We move the points slightly towards the building center
        Vector2 dirToCenter1 = Vector2Normalize(Vector2Subtract(bCenter, rawP1));
        Vector2 dirToCenter2 = Vector2Normalize(Vector2Subtract(bCenter, rawP2));
        
        Vector2 p1 = Vector2Add(rawP1, Vector2Scale(dirToCenter1, shrinkAmount));
        Vector2 p2 = Vector2Add(rawP2, Vector2Scale(dirToCenter2, shrinkAmount));


        float dist = Vector2Distance(p1, p2);
        if (dist < 0.5f) continue;


        Vector2 dir = Vector2Normalize(Vector2Subtract(p2, p1));
        Vector2 wallNormal = { -dir.y, dir.x };
        float angle = atan2f(dir.y, dir.x) * RAD2DEG; 
        float modelRotation = -angle;


        // Check orientation relative to center to ensure walls face outward
        Vector2 wallMid = Vector2Scale(Vector2Add(p1, p2), 0.5f);
        Vector2 toCenter = Vector2Subtract(bCenter, wallMid);
        
        // If normal points INWARD, flip it.
        if (Vector2DotProduct(wallNormal, toCenter) > 0) {
            wallNormal = Vector2Negate(wallNormal); 
            modelRotation += 180.0f;                
        }


        // Corner placement (using inset positions)
        // We push the corner slightly 'out' along the normal to cover the gap 
        // created by the shrink, but only visually.
        Vector3 cornerPos = { p1.x, visualHeight/2.0f, p1.y };
        BakeObjectToSector(ASSET_CORNER, cornerPos, -angle, ((Vector3){cornerThick, visualHeight, cornerThick}), tint);


        float moduleWidth = 2.0f * (MODEL_SCALE / 4.0f); 
        int modulesCount = (int)(dist / moduleWidth);
        float remainingSpace = dist - (modulesCount * moduleWidth);
        float startOffset = (remainingSpace / 2.0f) + (moduleWidth / 2.0f);
        
        Vector2 currentPos2D = Vector2Add(p1, Vector2Scale(dir, startOffset)); 
        
        float outwardOffset = 0.35f; 
        float beamHeight = 0.3f;
        float beamDepth = structuralDepth * 0.25f; 
        Vector3 floorBeamScale = { moduleWidth * 1.05f, beamHeight, beamDepth };
        
        for (int m = 0; m < modulesCount; m++) {
            for (int f = 0; f < floors; f++) {
                float yPos = (f * floorHeight) + 0.1f;
                Vector3 pos = { currentPos2D.x + (wallNormal.x * outwardOffset), yPos, currentPos2D.y + (wallNormal.y * outwardOffset) };
                
                bool isDoor = (f == 0 && m == modulesCount / 2);
                if (isDoor) {
                    Vector3 frameScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                    BakeObjectToSector(style.doorFrame, pos, modelRotation, frameScale, tint);
                    Vector3 doorScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth * 0.8f };
                    BakeObjectToSector(style.doorInner, pos, modelRotation, doorScale, tint);
                } else {
                    Vector3 winScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                    AssetType winType = (f == floors - 1) ? style.windowTop : style.window;
                    BakeObjectToSector(winType, pos, modelRotation, winScale, tint);
                    
                    if (f < floors-1 && GetRandomValue(0, 100) < 15) {
                        AssetType acType = (GetRandomValue(0, 1) == 0) ? ASSET_AC_A : ASSET_AC_B;
                        Vector3 acPos = { pos.x, pos.y - 0.4f, pos.z }; 
                        Vector3 acScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                        BakeObjectToSector(acType, acPos, modelRotation, acScale, tint);
                    }
                    
                }
                
                if (!style.isSkyscraper) {
                    if (f > 0) {
                        Vector3 beamPos = { pos.x, (f * floorHeight), pos.z };
                        BakeObjectToSector(ASSET_WALL, beamPos, modelRotation, floorBeamScale, tint);
                    }
                }
                
                if (f == floors - 1) {
                    float corniceOffset = 0.15f; 
                    Vector3 cornicePos = { currentPos2D.x + (wallNormal.x * corniceOffset), visualHeight, currentPos2D.y + (wallNormal.y * corniceOffset) };
                    Vector3 corniceScale = { moduleWidth * 1.05f, beamHeight, structuralDepth };
                    BakeObjectToSector(ASSET_WALL, cornicePos, modelRotation, corniceScale, tint);
                }
            }
            currentPos2D = Vector2Add(currentPos2D, Vector2Scale(dir, moduleWidth));
        }
        
        // Fill gaps at ends of wall
        if (remainingSpace > 0.1f) {
            float fillerLen = remainingSpace / 2.0f;
            Vector2 f1 = Vector2Add(p1, Vector2Scale(dir, fillerLen/2.0f));
            Vector3 pos1 = {f1.x, visualHeight/2.0f, f1.y};
            BakeObjectToSector(ASSET_WALL, pos1, -angle, ((Vector3){fillerLen, visualHeight, structuralDepth}), tint);
            
            Vector2 f2 = Vector2Add(p2, Vector2Scale(dir, -fillerLen/2.0f));
            Vector3 pos2 = {f2.x, visualHeight/2.0f, f2.y};
            BakeObjectToSector(ASSET_WALL, pos2, -angle, ((Vector3){fillerLen, visualHeight, structuralDepth}), tint);
        }
    }
}


// Helper to draw a cluster of objects based on event type
void DrawEventCluster(MapEvent *evt) {
    if (!evt->active) return;
    if (evt->position.x == 0.0f && evt->position.y == 0.0f) return;


    int seed = (int)(evt->position.x * 100) + (int)(evt->position.y * 100);
    SetRandomSeed(seed); 


    Vector3 center = { evt->position.x, 0.0f, evt->position.y };
    float boundaryRadius = evt->radius * 0.5f; 


    // --- SCENE 1: ROADWORK (Barriers, Big Lights, Chaos) ---
    if (evt->type == EVENT_ROADWORK) {
        
        // 1. Perimeter Ring (Big Barriers + Cones)
        int perimeterCount = 8; 
        for (int k = 0; k < perimeterCount; k++) {
            float angleDeg = (k / (float)perimeterCount) * 360.0f;
            float angleRad = angleDeg * DEG2RAD;
            
            Vector3 pPos = {
                center.x + cosf(angleRad) * boundaryRadius,
                0.0f,
                center.z + sinf(angleRad) * boundaryRadius
            };
            //sign
            Vector3 signPos = { center.x, 1.8f, center.z + 2.5f }; // Slightly offset
        
        DrawModelEx(cityRenderer.signConstruction, signPos, (Vector3){0,1,0}, 0.0f, (Vector3){1,1,1}, WHITE);
        
        // Draw Legs Manually (Cylinders)
        // ... inside EVENT_ROADWORK ...
    
    // Draw Legs (Optimized)
    Vector3 legScale = { 1.0f, 1.8f, 1.0f };
    DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x - 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, legScale, WHITE);
    DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x + 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, legScale, WHITE);
            float rot = -angleDeg + 90.0f; 


            // Alternate: Big Barrier vs Cone
            if (k % 2 == 0) {
                 // [FIX] Scale 6.0f (5x larger), Rotated + 90 degrees
                 DrawModelEx(cityRenderer.models[ASSET_PROP_BARRIER], pPos, 
                    (Vector3){0,1,0}, rot + 90.0f, (Vector3){6.0f, 6.0f, 6.0f}, WHITE);
            } else {
                 DrawModel(cityRenderer.models[ASSET_PROP_CONE], pPos, 1.0f, WHITE);
            }
        }


        // 2. Interior Scatter (More lights and cones)
        for (int i = 0; i < 5; i++) {
            Vector3 rndPos = { 
                center.x + GetRandomValue(-3,3), 
                0.0f, 
                center.z + GetRandomValue(-3,3) 
            };
            
            // Big Construction Light
            if (i % 2 == 0) {
                DrawModelEx(cityRenderer.models[ASSET_PROP_CONST_LIGHT], rndPos, 
                    (Vector3){0,1,0}, GetRandomValue(0,360), (Vector3){6.0f, 6.0f, 6.0f}, WHITE);
            } 
            // Extra Cone
            else {
                DrawModel(cityRenderer.models[ASSET_PROP_CONE], rndPos, 1.0f, WHITE);
            }
        }
        
        // Center Box
        DrawModel(cityRenderer.models[ASSET_PROP_BOX], center, 2.0f, WHITE);
    } 
    
    // --- SCENE 2: CRASH (Police, Separate Cars, Cones only) ---
    else if (evt->type == EVENT_CRASH) {
        
        // 1. Perimeter (Cones ONLY, no barriers)
        int perimeterCount = 10;
        for (int k = 0; k < perimeterCount; k++) {
            float angleRad = (k / (float)perimeterCount) * 360.0f * DEG2RAD;
            Vector3 pPos = {
                center.x + cosf(angleRad) * boundaryRadius,
                0.0f,
                center.z + sinf(angleRad) * boundaryRadius
            };
            DrawModel(cityRenderer.models[ASSET_PROP_CONE], pPos, 0.5f, WHITE);
        }
        //sign
        Vector3 signPos = { center.x + 3.0f, 1.8f, center.z }; 
        float rot = 90.0f; // Face the road usually

        DrawModelEx(cityRenderer.signAccident, signPos, (Vector3){0,1,0}, rot, (Vector3){1,1,1}, WHITE);
        
        // Legs
        // ... inside EVENT_ROADWORK ...
    
    // Draw Legs (Optimized)
    Vector3 legScale = { 1.0f, 1.8f, 1.0f };
    DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x - 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, legScale, WHITE);
    DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x + 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, legScale, WHITE);

        // 2. Crashed Cars (Pushed apart)
        int car1Type = ASSET_CAR_DELIVERY + GetRandomValue(0, 4);
        int car2Type = ASSET_CAR_DELIVERY + GetRandomValue(0, 4);
        
        float baseRot = GetRandomValue(0, 360);
        
        // [FIX] Specific offsets to prevent clipping (approx 3.5 units apart)
        Vector3 pos1 = { center.x + 1.8f, 0.0f, center.z + 1.8f };
        Vector3 pos2 = { center.x - 1.8f, 0.0f, center.z - 1.8f };
        
        DrawModelEx(cityRenderer.models[car1Type], pos1, (Vector3){0,1,0}, baseRot, (Vector3){1,1,1}, WHITE);
        DrawModelEx(cityRenderer.models[car2Type], pos2, (Vector3){0,1,0}, baseRot + 90.0f, (Vector3){1,1,1}, GRAY);


        // 3. Police Car
        Vector3 policePos = { center.x + 3.0f, 0.0f, center.z - 3.0f };
        // Rotate to face the crash
        Vector3 toCenter = Vector3Subtract(center, policePos);
        float policeRot = atan2f(toCenter.x, toCenter.z) * RAD2DEG;
        
        DrawModelEx(cityRenderer.models[ASSET_CAR_POLICE], policePos, (Vector3){0,1,0}, policeRot, (Vector3){1,1,1}, WHITE);
    }
}


// Helper to prevent props from clipping into building walls
bool IsTooCloseToBuilding(GameMap *map, Vector2 pos, float minDistance) {
    // Optimization: Only check buildings within a rough distance first
    for (int i = 0; i < map->buildingCount; i++) {
        Building *b = &map->buildings[i];
        
        // Quick bounding box check
        if (pos.x < b->footprint[0].x - 50 && pos.x > b->footprint[0].x + 50) continue;
        
        // Detailed check
        if (CheckCollisionPointPoly(pos, b->footprint, b->pointCount)) return true;
        
        // Distance check to edges (simplified as checking point distance to vertices)
        for(int k=0; k<b->pointCount; k++) {
             if (Vector2Distance(pos, b->footprint[k]) < minDistance) return true;
        }
    }
    return false;
}


void BakeRoadDetails(GameMap *map) {
    float sidewalkW = 2.5f; 
    float lightSpacing = 16.0f; 


    // 1. Calculate Node Degrees
    int *nodeDegree = (int*)calloc(map->nodeCount, sizeof(int));
    for(int i=0; i<map->edgeCount; i++) {
        if(map->edges[i].startNode < map->nodeCount) nodeDegree[map->edges[i].startNode]++;
        if(map->edges[i].endNode < map->nodeCount) nodeDegree[map->edges[i].endNode]++;
    }


    Color treeTint = (Color){40, 110, 40, 255};
    Color benchTint = (Color){100, 70, 40, 255};
    Color trashTint = (Color){50, 50, 60, 255};
    Color lightTint = WHITE; 
    Color sidewalkTint = (Color){180, 180, 180, 255};


    for(int i=0; i<map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;
        float finalRoadW = (e.width * MAP_SCALE) * 2.0f;
        Vector2 dir = Vector2Normalize(Vector2Subtract(en, s));
        Vector2 right = { -dir.y, dir.x };
        float len = Vector2Distance(s, en);
        float angle = atan2f(dir.y, dir.x) * RAD2DEG; 


        float offsetDist = (finalRoadW / 2.0f) + (sidewalkW / 2.0f);
        
        // Corner cutting
        float cutFactor = finalRoadW * 0.55f; 
        float startCut = (nodeDegree[e.startNode] > 2) ? cutFactor : 0.0f;
        float endCut = (nodeDegree[e.endNode] > 2) ? cutFactor : 0.0f;
        
        if (startCut + endCut > len * 0.9f) {
            float factor = (len * 0.9f) / (startCut + endCut);
            startCut *= factor; endCut *= factor;
        }


        for(int side=-1; side<=1; side+=2) {
            Vector2 sideOffset = Vector2Scale(right, offsetDist * side);
            Vector2 rawStart = Vector2Add(s, sideOffset);
            Vector2 swStart = Vector2Add(rawStart, Vector2Scale(dir, startCut));
            float swLen = len - (startCut + endCut);
            
            if (swLen < 0.1f) continue;


            // Bake Sidewalk Mesh
            Vector2 swMid = Vector2Add(swStart, Vector2Scale(dir, swLen/2.0f));
            Vector3 swPos = { swMid.x, 0.07f, swMid.y }; 
            Vector3 swScale = { swLen, 0.10f, sidewalkW };
            BakeObjectToSector(ASSET_SIDEWALK, swPos, -angle, swScale, sidewalkTint);


            // --- Organized Prop Generation ---
            float currentDist = 0.0f;
            float nextLight = lightSpacing * 0.5f; 


            while (currentDist < swLen) {
                Vector2 propPos2D = Vector2Add(swStart, Vector2Scale(dir, currentDist));
                Vector3 propPos = { propPos2D.x, 0.2f, propPos2D.y };
                
                // 1. Check Location Collisions (Fuel/Mech)
                bool blocked = false;
                for(int L=0; L<map->locationCount; L++) {
                    if(map->locations[L].type != LOC_HOUSE && map->locations[L].type != LOC_FUEL) {
                          if(Vector2Distance(propPos2D, map->locations[L].position) < 6.0f) blocked = true;
                    }
                }


                // 2. Check Building Clipping (New Check)
                if (!blocked && IsTooCloseToBuilding(map, propPos2D, 3.0f)) {
                    blocked = true;
                }
                
                if (!blocked) {
                    // Street Lights
                    if (currentDist >= nextLight) {
                        float lightRot = -angle; 
                        if (side == 1) lightRot += 90.0f; 
                        else lightRot -= 90.0f; 
                        
                        // User Request: Rotate 90 clockwise (-90)
                        lightRot -= 90.0f;


                        // User Request: "Just a touch larger" (was 2.4f)
                        Vector3 lScale = { 2.8f, 2.8f, 2.8f }; 
                        BakeObjectToSector(ASSET_PROP_LIGHT_CURVED, propPos, lightRot, lScale, lightTint);
                        
                        nextLight += lightSpacing;
                    } 
                    // Random Clutter
                    else {
                        int roll = GetRandomValue(0, 100);
                        float baseRot = (side == 1) ? -angle : -angle + 180.0f;


                        if (roll < 3) { 
                            // Trash Can
                            Vector3 tScale = { 1.2f, 1.2f, 1.2f };
                            BakeObjectToSector(ASSET_PROP_TRASH, propPos, baseRot, tScale, trashTint);
                        }
                        else if (roll < 8) {
                            // Bench
                            // User Request: Rotate 180 (So we remove the +180 offset, or add 180 to it)
                            // Previous was: baseRot + 180.0f. New: baseRot.
                            Vector3 bScale = { 1.5f, 1.5f, 1.5f };
                            BakeObjectToSector(ASSET_PROP_BENCH, propPos, baseRot, bScale, benchTint);
                        }
                        else if (roll < 9) {
                            // Cone Cluster (Probability reduced from 12 to 9)
                            Vector3 cScale = { 1.0f, 1.0f, 1.0f };
                            BakeObjectToSector(ASSET_PROP_CONE, propPos, 0, cScale, WHITE);
                            Vector3 c2 = {propPos.x + 0.5f, propPos.y, propPos.z + 0.5f};
                            BakeObjectToSector(ASSET_PROP_CONE, c2, 0, cScale, WHITE);
                        }
                        else if (roll < 20) {
                            // Small Tree
                            // User Request: 5x Larger (Prev 0.9f -> 4.5f)
                            Vector3 tScale = { 4.5f, 4.5f, 4.5f };
                            BakeObjectToSector(ASSET_PROP_TREE_SMALL, propPos, GetRandomValue(0,360), tScale, treeTint);
                        }
                        else if (roll < 35) {
                            // Grass/Flowers
                            if (GetRandomValue(0,1) == 0) {
                                Vector3 gScale = { 1.5f, 1.0f, 1.5f };
                                BakeObjectToSector(ASSET_PROP_GRASS, propPos, GetRandomValue(0,360), gScale, treeTint);
                            } else {
                                Vector3 fScale = { 1.2f, 1.0f, 1.2f };
                                BakeObjectToSector(ASSET_PROP_FLOWERS, propPos, GetRandomValue(0,360), fScale, WHITE);
                            }
                        }
                    }
                }
                currentDist += 2.0f;
            }
        }
    }
    free(nodeDegree);
}


void ClearEvents(GameMap *map) {
    for (int i = 0; i < MAX_EVENTS; i++) {
        map->events[i].active = false;
        map->events[i].timer = 0;
    }
}


void TriggerSpecificEvent(GameMap *map, MapEventType type, Vector3 playerPos, Vector3 playerFwd) {
    // [AMENDMENT 1] Clear all other events first.
    // This ensures the Tutorial event is the only one the player sees.
    ClearEvents(map); 


    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!map->events[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;


    Vector2 spawnPos;
    // [AMENDMENT 2] Increased distance to 20.0f to give player reaction time
    spawnPos.x = playerPos.x + (playerFwd.x * 20.0f);
    spawnPos.y = playerPos.z + (playerFwd.z * 20.0f);


    map->events[slot].active = true;
    map->events[slot].type = type;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f; 
    
    // [AMENDMENT 3] Increased duration to 60s so it persists during tutorial reading
    map->events[slot].timer = 60.0f; 


    if (type == EVENT_CRASH) {
        snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    } else if (type == EVENT_ROADWORK) {
        snprintf(map->events[slot].label, 64, "ROAD WORK");
    }
}


void TriggerRandomEvent(GameMap *map, Vector3 playerPos, Vector3 playerFwd) {
    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!map->events[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return; 


    int attempts = 0;
    bool found = false;
    Vector2 spawnPos = {0};


    while (attempts < 50) {
        int eIdx = GetRandomValue(0, map->edgeCount - 1);
        Edge e = map->edges[eIdx];
        
        Vector2 p1 = map->nodes[e.startNode].position;
        Vector2 p2 = map->nodes[e.endNode].position;
        Vector2 mid = Vector2Scale(Vector2Add(p1, p2), 0.5f); 
        
        float dist = Vector2Distance(mid, (Vector2){playerPos.x, playerPos.z});
        
        if (dist > 100.0f && dist < 500.0f) {
            spawnPos = mid;
            found = true;
            break;
        }
        attempts++;
    }


    if (!found) return; 


    map->events[slot].active = true;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f;
    map->events[slot].timer = 120.0f; 


    if (GetRandomValue(0, 100) < 50) {
        map->events[slot].type = EVENT_CRASH;
        snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    } else {
        map->events[slot].type = EVENT_ROADWORK;
        snprintf(map->events[slot].label, 64, "ROAD WORK");
    }
}


void UpdateDevControls(GameMap *map, Player *player) {
    
    // Calculate forward vector based on player angle (for spawning events in front)
    Vector3 fwd = { sinf(player->angle * DEG2RAD), 0, cosf(player->angle * DEG2RAD) };


    // F1: Spawn Crash
    if (IsKeyPressed(KEY_F1)) {
        TriggerSpecificEvent(map, EVENT_CRASH, player->position, fwd);
        TraceLog(LOG_INFO, "DEV: Spawned Crash");
    }


    // F2: Spawn Roadwork
    if (IsKeyPressed(KEY_F2)) {
        TriggerSpecificEvent(map, EVENT_ROADWORK, player->position, fwd);
        TraceLog(LOG_INFO, "DEV: Spawned Roadwork");
    }


    // F4: Clear All Events
    if (IsKeyPressed(KEY_F4)) {
        ClearEvents(map);
        TraceLog(LOG_INFO, "DEV: Cleared Events");
    }


    // F7: Open Dealership [NEW]
    if (IsKeyPressed(KEY_F7)) {
        EnterDealership(player);
        TraceLog(LOG_INFO, "DEV: Forced Dealership Entry");
    }
}


static void DrawCenteredLabel(Camera camera, Vector3 position, const char *text, Color color) {
    // [FIX] Check if the point is actually in front of the camera
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 toPos = Vector3Subtract(position, camera.position);
    
    // If dot product is negative, the object is behind us. Don't draw.
    if (Vector3DotProduct(forward, toPos) < 0) return;


    Vector2 screenPos = GetWorldToScreen(position, camera);
    
    // Boundary check to ensure it's within the screen area
    if (screenPos.x > 0 && screenPos.x < GetScreenWidth() && 
        screenPos.y > 0 && screenPos.y < GetScreenHeight()) {
        
        int fontSize = 20;
        int textW = MeasureText(text, fontSize);
        int padding = 4;
        
        DrawRectangle(screenPos.x - textW/2 - padding, screenPos.y - fontSize/2 - padding, 
                      textW + padding*2, fontSize + padding*2, Fade(BLACK, 0.6f));
                      
        DrawText(text, (int)screenPos.x - textW/2, (int)screenPos.y - fontSize/2, fontSize, WHITE);
    }
}


// --- RENDER ---
void DrawGameMap(GameMap *map, Camera camera) {
    UpdateMapStreaming(map, camera.position);
    
    Vector2 pPos2D = { camera.position.x, camera.position.z };
    
    // 1. Infinite Floor
    DrawPlane((Vector3){0, -0.05f, 0}, (Vector2){10000.0f, 10000.0f}, (Color){80, 80, 80, 255});
    
    // Only draw dynamic parks if close (Performance Optimization)
    // Dynamic parks use immediate mode drawing which is slow
    if (GetFPS() > 30) {
        DrawRuntimeParks(camera.position);
    }
    DrawMapBoundaries(camera.position);
    // 2. Visible Range
    int range = (int)(RENDER_DIST_BASE / GRID_CELL_SIZE) + 1; 
    int camGridX = (int)((pPos2D.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int camGridY = (int)((pPos2D.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);

    int minX = camGridX - range; if (minX < 0) minX = 0;
    int maxX = camGridX + range; if (maxX >= SECTOR_GRID_COLS) maxX = SECTOR_GRID_COLS - 1;
    int minY = camGridY - range; if (minY < 0) minY = 0;
    int maxY = camGridY + range; if (maxY >= SECTOR_GRID_ROWS) maxY = SECTOR_GRID_ROWS - 1;

    // Calculate Camera Forward Vector for Culling
    Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    // 3. Draw Active Sectors
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
             Sector *sec = &cityRenderer.sectors[y][x];
             if (sec->active && !sec->isEmpty) {
                 
                 // [OPTIMIZATION] Simple Frustum Culling
                 // Check if sector is behind the camera
                 Vector3 secCenter = { 
                     (x * GRID_CELL_SIZE) - SECTOR_WORLD_OFFSET + (GRID_CELL_SIZE/2), 
                     0, 
                     (y * GRID_CELL_SIZE) - SECTOR_WORLD_OFFSET + (GRID_CELL_SIZE/2) 
                 };

                 Vector3 dirToSec = Vector3Subtract(secCenter, camera.position);
                 
                 // If sector is very close (within 80m), draw it regardless of angle 
                 // (fixes disappearing chunks when looking down/rotating fast)
                 if (Vector3LengthSqr(dirToSec) > 80.0f*80.0f) {
                     // If dot product is negative, it's behind us -> Skip
                     if (Vector3DotProduct(camFwd, Vector3Normalize(dirToSec)) < -0.2f) continue;
                 }

                 DrawModel(sec->model, (Vector3){0,0,0}, 1.0f, WHITE);
             }
        }
    }
    
    // ... (Keep existing locations/events drawing logic) ...
    // Note: Ensure the rest of the function remains the same as previous steps
    // 4. Draw Locations (Standard)
    // 4. Draw Locations (Standard)
    for (int i = 0; i < map->locationCount; i++) {
        // Distance check
        if (Vector2Distance(pPos2D, map->locations[i].position) > RENDER_DIST_BASE) continue;
        
        Vector3 pos = { map->locations[i].position.x, 0.0f, map->locations[i].position.y };
        float dx = pos.x - camera.position.x;
        float dz = pos.z - camera.position.z;
        float distSqr = dx*dx + dz*dz;

        // --- FUEL STATION ---
        if (map->locations[i].type == LOC_FUEL) {
            // 1. Fuel Pump (Yellow Box with Black Top)
            Vector3 pumpPos = { pos.x, 1.0f, pos.z };
            DrawCube(pumpPos, 1.2f, 2.0f, 1.2f, YELLOW);
            DrawCubeWires(pumpPos, 1.2f, 2.0f, 1.2f, DARKGRAY);
            DrawCube((Vector3){pumpPos.x, 2.1f, pumpPos.z}, 1.3f, 0.2f, 1.3f, BLACK); // Roof cap

            // 2. Sign (Offset slightly)
            // Use the legs logic to draw a sign next to it
            Vector3 signPos = { pos.x + 2.0f, 1.8f, pos.z };
            // Draw Legs
            DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x - 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, (Vector3){1, 1.8f, 1}, WHITE);
            DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x + 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, (Vector3){1, 1.8f, 1}, WHITE);
            // Draw Board (Reusing RoadWork sign for now, you can load a "FUEL" texture later)
            DrawModelEx(cityRenderer.signConstruction, signPos, (Vector3){0,1,0}, 0.0f, (Vector3){1,1,1}, WHITE);

            // 3. Interaction Prompt
            if (distSqr < 144.0f) { // 12m range
                DrawCenteredLabel(camera, (Vector3){pos.x, 2.5f, pos.z}, "Refuel [E]", YELLOW);
            }
        }
        
        // --- MECHANIC SHOP ---
        else if (map->locations[i].type == LOC_MECHANIC) {
            // 1. Mechanic Stand (Blue Workbench area)
            Vector3 benchPos = { pos.x, 0.5f, pos.z };
            DrawCube(benchPos, 3.0f, 1.0f, 1.0f, DARKBLUE); // Table
            DrawCube((Vector3){pos.x, 1.5f, pos.z}, 3.0f, 0.1f, 1.0f, GRAY); // Shelf

            // 2. Sign (Vertical "Repair" Sign)
            Vector3 signPos = { pos.x - 2.5f, 1.8f, pos.z };
            DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x, 0.9f, signPos.z}, (Vector3){0,1,0}, 0.0f, (Vector3){1, 1.8f, 1}, WHITE);
            // Reusing Accident sign for Mechanic
            DrawModelEx(cityRenderer.signAccident, signPos, (Vector3){0,1,0}, 0.0f, (Vector3){1,1,1}, WHITE);

            // 3. Interaction Prompt
            if (distSqr < 144.0f) {
                DrawCenteredLabel(camera, (Vector3){pos.x, 2.5f, pos.z}, "Mechanic [E]", SKYBLUE);
            }
        }

        // --- DEALERSHIP ---
        else if (map->locations[i].type == LOC_DEALERSHIP) {
            // 1. Rotating Display Car
            // We use a predefined sedan model or a fallback box if models aren't loaded
            Vector3 carPos = { pos.x, 0.5f, pos.z };
            float spin = (float)GetTime() * 30.0f;
            
            // Draw a podium
            DrawCylinder((Vector3){carPos.x, 0.1f, carPos.z}, 2.5f, 2.5f, 0.2f, 16, DARKGRAY);
            
            // Draw the car (Use your specific model index for ASSET_CAR_SEDAN)
            // If you want to use the low-poly procedural car here, you'd need to extract that logic.
            // For now, we assume the Model asset is available.
            DrawModelEx(cityRenderer.models[ASSET_CAR_SEDAN], carPos, (Vector3){0,1,0}, spin, (Vector3){1.2f, 1.2f, 1.2f}, WHITE);

            // 2. Dealership Sign
            Vector3 signPos = { pos.x + 3.5f, 1.8f, pos.z + 2.0f };
            // Legs
            DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x - 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 45.0f, (Vector3){1, 1.8f, 1}, WHITE);
            DrawModelEx(cityRenderer.signLegModel, (Vector3){signPos.x + 0.8f, 0.9f, signPos.z}, (Vector3){0,1,0}, 45.0f, (Vector3){1, 1.8f, 1}, WHITE);
            // Board (Reusing RoadClosed as "Dealership")
            DrawModelEx(cityRenderer.signRoadClosed, signPos, (Vector3){0,1,0}, 45.0f, (Vector3){1,1,1}, WHITE);
        }
    }
    
    // 5. Draw Events
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            if (Vector2Distance(pPos2D, map->events[i].position) <= RENDER_DIST_BASE) {
                DrawEventCluster(&map->events[i]);
            }
        }
    }
    
    rlEnableBackfaceCulling();
    EndMode3D(); 
    
    // 6. Draw UI Labels
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
             if (Vector2Distance(pPos2D, map->events[i].position) < 40.0f) {
                Vector3 pos = { map->events[i].position.x, 3.5f, map->events[i].position.y };
                DrawCenteredLabel(camera, pos, map->events[i].label, COLOR_EVENT_TEXT);
             }
        }
    }
    
    // Interaction Prompts
    for (int i = 0; i < map->locationCount; i++) {
        if (map->locations[i].type == LOC_FUEL || map->locations[i].type == LOC_MECHANIC|| map->locations[i].type == LOC_DEALERSHIP) {
            if (Vector2Distance(pPos2D, map->locations[i].position) > 50.0f) continue;

            Vector3 targetPos = { map->locations[i].position.x + 2.0f, 1.5f, map->locations[i].position.y + 2.0f };
            if (Vector3DistanceSqr(targetPos, camera.position) < 144.0f) { 
                const char* txt = 0;
                switch (map->locations[i].type)
                {
                case LOC_FUEL:
                    txt = "Refuel [E]";
                    break;
                case LOC_MECHANIC: 
                    txt = "Mechanic [E]";
                    break;
                case LOC_DEALERSHIP: 
                    txt = "Enter Dealership [E]";
                    break;
                default:
                    txt = "Press [E] to interact";
                    break;
                }
                
                DrawCenteredLabel(camera, targetPos, txt, YELLOW);
            }
        }
    }

    BeginMode3D(camera); 
}

void UpdateMapEffects(GameMap *map, Vector3 playerPos) {
    for(int i=0; i<MAX_EVENTS; i++) {
        if(map->events[i].active) {
            map->events[i].timer -= GetFrameTime();
            if(map->events[i].timer <= 0) map->events[i].active = false;
        }
    }
}

// Put this in map.c (before DrawZoneMarker)
Vector3 GetSmartDeliveryPos(GameMap *map, Vector3 buildingCenter) {
    // 1. Find the nearest road node
    int roadNodeIdx = GetClosestNode(map, (Vector2){buildingCenter.x, buildingCenter.z});
    
    if (roadNodeIdx != -1) {
        // 2. Get direction: Building -> Road
        Vector2 roadPos = map->nodes[roadNodeIdx].position;
        Vector2 bPos = { buildingCenter.x, buildingCenter.z };
        
        Vector2 dir = Vector2Subtract(roadPos, bPos);
        dir = Vector2Normalize(dir);

        // 3. Push OUT towards the road by 15 units
        Vector2 offsetPos = Vector2Add(bPos, Vector2Scale(dir, 5.0f));
        
        // Return the new "Street-Side" position (Height 0.5f)
        return (Vector3){ offsetPos.x, 1.0f, offsetPos.y };
    }
    
    // Fallback: Just return center if no road found
    return buildingCenter;
}

// Define custom colors for the package look just above the function
#define COLOR_CARDBOARD (Color){ 170, 130, 100, 255 } // Brownish tan
#define COLOR_TAPE      (Color){ 200, 180, 150, 255 } // Lighter tan

// In map.c

void DrawZoneMarker(GameMap *map, Vector3 drawPos, Color color) {
    float time = GetTime();

    // Determine if this is a Pickup or Dropoff based on color
    // LIME (Pickup) has high Green. ORANGE (Dropoff) has high Red.
    bool isDropOff = (color.r > color.g); 

    // --- ANIMATION CALCULATIONS ---
    float bob = sinf(time * 3.0f) * 0.15f;
    float spinAngle = time * 50.0f;
    Vector3 finalPos = { drawPos.x, drawPos.y + 0.4f + bob, drawPos.z };

    // --- PACKAGE DIMENSIONS ---
    float w = 0.6f;
    float h = 0.4f;
    float d = 0.5f;

    // 1. DRAW GROUND INDICATOR (The Ring)
    // Pulsing effect for the ring
    float pulse = (sinf(time * 5.0f) + 1.0f) * 0.5f; // 0.0 to 1.0
    float ringAlpha = isDropOff ? 0.3f + (pulse * 0.3f) : 0.6f;
    
    DrawCircle3D((Vector3){finalPos.x, 0.08f, finalPos.z}, 0.8f, (Vector3){1,0,0}, 90.0f, Fade(color, ringAlpha));
    
    // Beacon Line
    DrawLine3D(finalPos, (Vector3){finalPos.x, 15.0f, finalPos.z}, Fade(color, 0.25f));

    // 2. DRAW THE BOX
    rlPushMatrix();
        rlTranslatef(finalPos.x, finalPos.y, finalPos.z);
        rlRotatef(15.0f, 1.0f, 0.0f, 0.0f);
        rlRotatef(spinAngle, 0.0f, 1.0f, 0.0f);

        if (isDropOff) {
            // [GHOST STYLE] - For Drop-off
            // Draws a "Wireframe Blueprint" to show where the box goes
            
            // Pulsing transparency
            float alpha = 0.2f + (pulse * 0.2f);
            
            // Faint inner fill
            DrawCube(Vector3Zero(), w, h, d, Fade(color, alpha));
            // Bright wireframe outline
            DrawCubeWires(Vector3Zero(), w + 0.02f, h + 0.02f, d + 0.02f, color);
            
            // "PLACE HERE" Text effect (Floating arrow or just the wires is usually enough)
        } 
        else {
            // [REALISTIC STYLE] - For Pickup
            // Draws the actual cardboard box
            
            // Main Cardboard Body
            DrawCube(Vector3Zero(), w, h, d, COLOR_CARDBOARD);
            DrawCubeWires(Vector3Zero(), w + 0.01f, h + 0.01f, d + 0.01f, DARKBROWN);

            // Shipping Label
            DrawCube((Vector3){0, h / 2.0f + 0.01f, 0}, w * 0.7f, 0.01f, d * 0.7f, RAYWHITE);

            // Tape
            DrawCube(Vector3Zero(), w + 0.02f, h * 0.15f, d + 0.02f, COLOR_TAPE);
        }

    rlPopMatrix();

    // Shadow (Only for real box)
    if (!isDropOff) {
        DrawCylinder((Vector3){finalPos.x, 0.05f, finalPos.z}, 0.5f, 0.5f, 0.02f, 16, Fade(BLACK, 0.3f));
    }
}
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


// [OPTIMIZATION] Fast Collision Check
bool CheckMapCollision(GameMap *map, float x, float z, float radius, bool isCamera) {
    // 1. Determine which cell the object is in
    int gx = (int)((x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int gy = (int)((z + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);


    Vector2 p = { x, z };


    // 2. Check 3x3 grid around player (to handle straddling borders)
    for (int cy = gy - 1; cy <= gy + 1; cy++) {
        for (int cx = gx - 1; cx <= gx + 1; cx++) {
            
            // Bounds check
            if (cx < 0 || cx >= SECTOR_GRID_COLS || cy < 0 || cy >= SECTOR_GRID_ROWS) continue;


            CollisionCell *cell = &colGrid[cy][cx];
            
            // 3. Check ONLY buildings in this cell
            for (int k = 0; k < cell->count; k++) {
                int bIdx = cell->indices[k];
                Building *b = &map->buildings[bIdx];


                // Precise Poly Check
                if (CheckCollisionPointPoly(p, b->footprint, b->pointCount)) return true;
            }
        }
    }

    if (CheckBoundaryCollision((Vector3){x, 0, z}, radius)) return true;
    // 4. Check Events (Keep this global as there are few of them)
    if (!isCamera){
        for(int i=0; i<MAX_EVENTS; i++) {
            if (map->events[i].active) {
                // [FIX] Reduced hitbox by 50% (* 0.5f) to match the new visual barriers
                float effectiveRadius = (map->events[i].radius * 0.5f) + radius;
                if (Vector2Distance(p, map->events[i].position) < effectiveRadius) return true;
            }
        }
    }


    return false;
}






void UnloadGameMap(GameMap *map) {
    // 1. Free Map Data Arrays
    if (map->nodes) free(map->nodes);
    if (map->edges) free(map->edges);
    
    if (map->buildingCount > 0) {
        for (int i = 0; i < map->buildingCount; i++) free(map->buildings[i].footprint);
    }
    if (map->buildings) free(map->buildings);


    if (map->areaCount > 0) {
        for (int i = 0; i < map->areaCount; i++) free(map->areas[i].points);
    }
    if (map->areas) free(map->areas);
    
    if (map->locations) free(map->locations);
    
    // 2. Free Graph
    if (map->graph) {
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }


    // 3. Unload Renderer
    if (cityRenderer.loaded) {
        cityRenderer.activeSectorCount = 0;
        // A. Unload SECTORS
        for (int y = 0; y < SECTOR_GRID_ROWS; y++) {
            for (int x = 0; x < SECTOR_GRID_COLS; x++) {
                UnloadSectorChunk(x, y); 
                
                if (cityRenderer.builders[y][x]) {
                    FreeSectorBuilder(cityRenderer.builders[y][x]);
                    free(cityRenderer.builders[y][x]);
                    cityRenderer.builders[y][x] = NULL;
                }
                
                SectorManifest *man = &cityRenderer.manifests[y][x];
                if (man->buildingIndices) { free(man->buildingIndices); man->buildingIndices = NULL; }
                if (man->edgeIndices) { free(man->edgeIndices); man->edgeIndices = NULL; }
                if (man->areaIndices) { free(man->areaIndices); man->areaIndices = NULL; }
                man->buildingCount = 0; man->buildingCap = 0;
                man->edgeCount = 0; man->edgeCap = 0;
                man->areaCount = 0; man->areaCap = 0;
            }
        }
        
        // B. Unload ASSETS
        // [FIX] Prevent Double-Free of Aliased Models
        // ASSET_CORNER and ASSET_SIDEWALK share memory with ASSET_WALL.
        // We zero them out so the loop skips them, letting ASSET_WALL handle the free.
        cityRenderer.models[ASSET_CORNER] = (Model){0};
        cityRenderer.models[ASSET_SIDEWALK] = (Model){0};


        for (int m = 0; m < ASSET_COUNT; m++) {
            UnloadModelSafe(cityRenderer.models[m]);
        }
        
        // C. Unload Legacy
        if (cityRenderer.mapBaked) {
            UnloadModelSafe(cityRenderer.roadModel);
            UnloadModelSafe(cityRenderer.markingsModel);
            UnloadModelSafe(cityRenderer.areaModel);
            UnloadModelSafe(cityRenderer.roofModel); 
        }


        // D. Unload Shared Texture
        if (cityRenderer.whiteTex.id != 0) {
            UnloadTexture(cityRenderer.whiteTex);
            cityRenderer.whiteTex = (Texture2D){0};
        }


        if (cityRenderer.nodeDegrees) {
            free(cityRenderer.nodeDegrees);
            cityRenderer.nodeDegrees = NULL;
        }


        cityRenderer.loaded = false;
        cityRenderer.mapBaked = false;
    }


    // 4. Free Collision Grid
    if (colGridLoaded) {
    for(int y=0; y<SECTOR_GRID_ROWS; y++) {
        for(int x=0; x<SECTOR_GRID_COLS; x++) {
            if (colGrid[y][x].indices) free(colGrid[y][x].indices);
            if (nodeGrid[y][x].indices) free(nodeGrid[y][x].indices); // FREE NODE GRID
        }
    }
    colGridLoaded = false;
}
    
}


// In map.c
void BuildMapGraph(GameMap *map) {
    if (map->graph) {
        // Safety: Free old graph if it exists to prevent memory leaks
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }


    map->graph = (NodeGraph*)calloc(map->nodeCount, sizeof(NodeGraph));
    
    for (int i = 0; i < map->edgeCount; i++) {
        int u = map->edges[i].startNode; 
        int v = map->edges[i].endNode;


        // Safety Check: Ensure connection is within bounds
        if (u >= map->nodeCount || v >= map->nodeCount) continue;


        float dist = Vector2Distance(map->nodes[u].position, map->nodes[v].position);


        // --- Connection U -> V ---
        if (map->graph[u].count >= map->graph[u].capacity) {
            map->graph[u].capacity = (map->graph[u].capacity == 0) ? 4 : map->graph[u].capacity * 2;
            map->graph[u].connections = realloc(map->graph[u].connections, map->graph[u].capacity * sizeof(GraphConnection));
        }
        // [FIX] We add 'i' (the edge index) so traffic knows which road this is
        map->graph[u].connections[map->graph[u].count++] = (GraphConnection){v, dist, i};


        // --- Connection V -> U (Only if Two-Way) ---
        if (!map->edges[i].oneway) {
            if (map->graph[v].count >= map->graph[v].capacity) {
                map->graph[v].capacity = (map->graph[v].capacity == 0) ? 4 : map->graph[v].capacity * 2;
                map->graph[v].connections = realloc(map->graph[v].connections, map->graph[v].capacity * sizeof(GraphConnection));
            }
            map->graph[v].connections[map->graph[v].count++] = (GraphConnection){u, dist, i};
        }
    }
    printf("Graph Rebuilt. Nodes: %d, Edges Processed: %d\n", map->nodeCount, map->edgeCount);
}


// [OPTIMIZATION] Fast Node Search
// [OPTIMIZATION] Spatial Search (O(1) instead of O(N))
int GetClosestNode(GameMap *map, Vector2 position) {
    int bestNode = -1; 
    float minDst = FLT_MAX;
    
    int gx = (int)((position.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int gy = (int)((position.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);


    // Search 3x3 cells around the position
    for (int y = gy - 1; y <= gy + 1; y++) {
        for (int x = gx - 1; x <= gx + 1; x++) {
            if (x < 0 || x >= SECTOR_GRID_COLS || y < 0 || y >= SECTOR_GRID_ROWS) continue;
            NodeCell *cell = &nodeGrid[y][x];
            for (int k = 0; k < cell->count; k++) {
                int nodeIdx = cell->indices[k];
                if (map->graph && map->graph[nodeIdx].count == 0) continue;
                float d = Vector2DistanceSqr(position, map->nodes[nodeIdx].position);
                if (d < minDst) { minDst = d; bestNode = nodeIdx; }
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
    for(int i=0; i<map->nodeCount; i++) { gScore[i] = FLT_MAX; fScore[i] = FLT_MAX; comeFrom[i] = -1; }
    int *openList = malloc(map->nodeCount * sizeof(int));
    int openCount = 0;
    gScore[startNode] = 0;
    fScore[startNode] = Vector2Distance(map->nodes[startNode].position, map->nodes[endNode].position);
    openList[openCount++] = startNode;
    inOpenSet[startNode] = true;
    int found = 0;
    while (openCount > 0) {
        int lowestIdx = 0;
        for(int i=1; i<openCount; i++) { if (fScore[openList[i]] < fScore[openList[lowestIdx]]) lowestIdx = i; }
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
                if (!inOpenSet[neighbor]) { openList[openCount++] = neighbor; inOpenSet[neighbor] = true; }
            }
        }
    }
    int pathLen = 0;
    if (found) {
        int curr = endNode;
        Vector2 *tempPath = malloc(maxPathLen * sizeof(Vector2));
        int count = 0;
        while (curr != -1 && count < maxPathLen) { tempPath[count++] = map->nodes[curr].position; curr = comeFrom[curr]; }
        for(int i=0; i<count; i++) outPath[i] = tempPath[count - 1 - i];
        pathLen = count;
        free(tempPath);
    }
    free(gScore); free(fScore); free(comeFrom); free(inOpenSet); free(openList);
    return pathLen;
}


// In map.c
void BuildNodeGrid(GameMap *map) {
    for(int y=0; y<SECTOR_GRID_ROWS; y++) {
        for(int x=0; x<SECTOR_GRID_COLS; x++) {
            if(nodeGrid[y][x].indices) free(nodeGrid[y][x].indices);
            nodeGrid[y][x].indices = NULL;
            nodeGrid[y][x].count = 0;
            nodeGrid[y][x].capacity = 0;
        }
    }
    for (int i = 0; i < map->nodeCount; i++) {
        Vector2 pos = map->nodes[i].position;
        int gx = (int)((pos.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        int gy = (int)((pos.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
        if (gx < 0 || gx >= SECTOR_GRID_COLS || gy < 0 || gy >= SECTOR_GRID_ROWS) continue;
        
        NodeCell *cell = &nodeGrid[gy][gx];
        if (cell->count >= cell->capacity) {
            cell->capacity = (cell->capacity == 0) ? 4 : cell->capacity * 2;
            cell->indices = (int*)realloc(cell->indices, cell->capacity * sizeof(int));
        }
        cell->indices[cell->count++] = i;
    }
    printf("Node Grid Built for %d nodes.\n", map->nodeCount);
}


GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    
    // --- Allocation ---
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));
    map.areas = (MapArea *)calloc(MAX_AREAS, sizeof(MapArea));
    map.isBatchLoaded = false;
    ClearMapBoundaries();
    ClearEvents(&map);
    LoadCityAssets(); 


    // --- File Parsing (Keep exactly as you have it) ---
    char *text = LoadFileText(fileName);
    if (!text) {
        printf("CRITICAL ERROR: Could not load map file %s\n", fileName);
        return map;
    }
    
    char *line = strtok(text, "\n");
    int mode = 0; 
    
    while (line != NULL) {
        // ... (Keep all your existing parsing logic for NODES, EDGES, BUILDINGS, etc.) ...
        // ... (This part of your code was fine) ...
        if (strncmp(line, "NODES:", 6) == 0) { mode = 1; }
        else if (strncmp(line, "EDGES:", 6) == 0) { mode = 2; }
        else if (strncmp(line, "BUILDINGS:", 10) == 0) { mode = 3; }
        else if (strncmp(line, "AREAS:", 6) == 0) { mode = 4; }
        else if (strncmp(line, "L ", 2) == 0) { 
             if (map.locationCount < MAX_LOCATIONS) {
                 int type; float x, y; char name[64];
                 if (sscanf(line, "L %d %f %f %63s", &type, &x, &y, name) == 4) {
                     map.locations[map.locationCount].position = (Vector2){ x * MAP_SCALE, y * MAP_SCALE };
                     if (type == 9) {
                        map.locations[map.locationCount].type = LOC_DEALERSHIP;
                    } else {
                        map.locations[map.locationCount].type = (LocationType)type;
                    }
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
                    map.nodes[map.nodeCount].flags = flags;
                    map.nodeCount++;
                }
            } else if (mode == 2 && map.edgeCount < MAX_EDGES) {
                int start, end, oneway, speed, lanes; float width;
                if (sscanf(line, "%d %d %f %d %d %d", &start, &end, &width, &oneway, &speed, &lanes) >= 3) {
                    map.edges[map.edgeCount].startNode = start;
                    map.edges[map.edgeCount].endNode = end;
                    map.edges[map.edgeCount].width = width * MAP_SCALE;
                    map.edges[map.edgeCount].oneway = oneway;
                    map.edges[map.edgeCount].maxSpeed = speed;
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
                    int pCount = 0; float px, py;
                    while (sscanf(ptr, "%f %f%n", &px, &py, &read) == 2 && pCount < MAX_BUILDING_POINTS) {
                        tempPoints[pCount] = (Vector2){px * MAP_SCALE, py * MAP_SCALE};
                        pCount++; ptr += read;
                    }
                    build->footprint = (Vector2 *)malloc(sizeof(Vector2) * pCount);
                    memcpy(build->footprint, tempPoints, sizeof(Vector2) * pCount);
                    build->pointCount = pCount;
                    if (pCount >= 3) map.buildingCount++;
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
                    int pCount = 0; float px, py;
                    while (sscanf(ptr, "%f %f%n", &px, &py, &read) == 2 && pCount < MAX_BUILDING_POINTS) {
                        tempPoints[pCount] = (Vector2){px * MAP_SCALE, py * MAP_SCALE};
                        pCount++; ptr += read;
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


    // --- CRITICAL FIX: REMOVED GLOBAL BAKING CALLS ---
    // The previous code had loops here that baked ALL buildings and ALL roads.
    // Those have been deleted. We now only build the manifests.

    
    printf("Map Data Loaded. Building Manifests...\n");
    
    // Sort all objects into their grid cells (Fast)
    BuildSectorManifests(&map);
    
    // Build physics/traffic data (Lightweight)
    BuildCollisionGrid(&map);
    BuildNodeGrid(&map);
    BuildMapGraph(&map);
    
    // --- PRE-LOAD STARTING ZONE (Synchronous Force Load) ---
    // We force the system to process all stages instantly for the starting area.
    int startX = (int)((0.0f + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    int startY = (int)((0.0f + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE);
    
    for (int y = startY - 1; y <= startY + 1; y++) {
        for (int x = startX - 1; x <= startX + 1; x++) {
            if (x >= 0 && x < SECTOR_GRID_COLS && y >= 0 && y < SECTOR_GRID_ROWS) {
                // Manually trigger the load state
                cityRenderer.loadingSectorX = x;
                cityRenderer.loadingSectorY = y;
                cityRenderer.isSectorLoading = true;
                cityRenderer.sectors[y][x].loadStage = 0;

                // Force loop until this sector returns false (Done)
                while(ProcessSectorLoadStep(&map));
            }
        }
    }


    printf("Map Ready.\n");
    return map;
}


// --- 2D MAP OPTIMIZATION ---
// Uses the Sector Grid to draw ONLY what is visible on the phone screen.
// This is 100x faster than looping through all buildings.
void DrawMap2DView(GameMap *map, Camera2D cam, float screenW, float screenH) {
    if (!cityRenderer.loaded) return;

    // 1. Calculate Visible World Bounds
    // We reverse the camera transform to find where the screen corners are in the world
    Vector2 topLeft = GetScreenToWorld2D((Vector2){0, 0}, cam);
    Vector2 bottomRight = GetScreenToWorld2D((Vector2){screenW, screenH}, cam);

    // 2. Convert to Grid Coordinates
    int minX = (int)((topLeft.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE) - 1;
    int minY = (int)((topLeft.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE) - 1;
    int maxX = (int)((bottomRight.x + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE) + 1;
    int maxY = (int)((bottomRight.y + SECTOR_WORLD_OFFSET) / GRID_CELL_SIZE) + 1;

    // Clamp to map bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= SECTOR_GRID_COLS) maxX = SECTOR_GRID_COLS - 1;
    if (maxY >= SECTOR_GRID_ROWS) maxY = SECTOR_GRID_ROWS - 1;

    float scale = 1.0f / cam.zoom;

    // 3. Iterate ONLY visible sectors
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            SectorManifest *man = &cityRenderer.manifests[y][x];

            // A. Draw Areas (Parks)
            for (int i = 0; i < man->areaCount; i++) {
                MapArea *area = &map->areas[man->areaIndices[i]];
                if (area->pointCount < 3) continue;
                Color areaColor = Fade(area->color, 0.4f);
                DrawTriangleFan(area->points, area->pointCount, areaColor);
                // Simplify outline for performance (skip thick lines if zoomed out)
                if (cam.zoom > 1.0f) {
                    DrawLineStrip(area->points, area->pointCount, areaColor);
                }
            }

            // B. Draw Roads
            for (int i = 0; i < man->edgeCount; i++) {
                int edgeIdx = man->edgeIndices[i];
                Edge e = map->edges[edgeIdx];
                Vector2 s = map->nodes[e.startNode].position;
                Vector2 en = map->nodes[e.endNode].position;
                
                // Draw Base Asphalt
                DrawLineEx(s, en, e.width, LIGHTGRAY);
                
                // Draw White Lines (Detail Level)
                if (e.width > 5.0f && cam.zoom > 2.0f) {
                    DrawLineEx(s, en, 1.0f * scale, WHITE);
                }
            }

            // C. Draw Buildings
            for (int i = 0; i < man->buildingCount; i++) {
                Building *b = &map->buildings[man->buildingIndices[i]];
                
                // LOD: If zoomed out far, just draw a simple rectangle/fan without outlines
                DrawTriangleFan(b->footprint, b->pointCount, Fade(b->color, 0.5f));
                
                // Only draw outlines if zoomed in
                if (cam.zoom > 1.5f) {
                    for(int j=0; j<b->pointCount; j++) {
                        Vector2 p1 = b->footprint[j];
                        Vector2 p2 = b->footprint[(j+1)%b->pointCount];
                        DrawLineEx(p1, p2, 2.0f * scale, DARKGRAY);
                    }
                }
            }
        }
    }
}



#include "map.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

// --- SAFETY DEFINITIONS (Guard against missing map.h definitions) ---
#ifndef MAX_BUILDING_POINTS
    #define MAX_BUILDING_POINTS 64
#endif
#ifndef MAX_NODES
    #define MAX_NODES 50000
#endif
#ifndef MAX_EDGES
    #define MAX_EDGES 50000
#endif
#ifndef MAX_BUILDINGS
    #define MAX_BUILDINGS 20000
#endif
#ifndef MAX_LOCATIONS
    #define MAX_LOCATIONS 1000
#endif
#ifndef MAX_AREAS
    #define MAX_AREAS 1000
#endif
#ifndef MAX_SEARCH_RESULTS
    #define MAX_SEARCH_RESULTS 32
#endif

// Fallback colors
#ifndef COLOR_ROAD
    #define COLOR_ROAD (Color){40, 40, 40, 255}
#endif
#ifndef COLOR_ROAD_MARKING
    #define COLOR_ROAD_MARKING (Color){200, 200, 200, 255}
#endif
#ifndef COLOR_PARK
    #define COLOR_PARK (Color){30, 120, 30, 255}
#endif
#ifndef COLOR_EVENT_PROP
    #define COLOR_EVENT_PROP (Color){255, 161, 0, 255}
#endif
#ifndef COLOR_EVENT_TEXT
    #define COLOR_EVENT_TEXT (Color){200, 200, 200, 255}
#endif

// --- CONFIGURATION ---
const float MAP_SCALE = 0.4f;

// [OPTIMIZATION] Grid & LOD Configuration
// The world is split into dynamic chunks (Props/Windows) and static sectors (Walls/Roads)

#define CHUNK_SIZE 64.0f         
#define WORLD_SIZE_METERS 6000.0f
#define GRID_WIDTH (int)(WORLD_SIZE_METERS / CHUNK_SIZE)
#define SECTOR_SIZE 128.0f       
#define SECTOR_WIDTH (int)(WORLD_SIZE_METERS / SECTOR_SIZE)

// LOD Distances
const float LOD_DIST_PROPS = 60.0f;    
const float LOD_DIST_WINDOWS = 120.0f;  
const float LOD_DIST_STATIC = 600.0f; // Distance for baked walls/roads

#define MODEL_SCALE 1.8f         
#define MODEL_Z_SQUISH 0.4f      
#define REGION_CENTER_RADIUS 600.0f

// --- TYPES ---

typedef enum {
    ASSET_AC_A = 0, ASSET_AC_B, ASSET_BALCONY, ASSET_BALCONY_WHITE,
    ASSET_DOOR_BROWN, ASSET_DOOR_BROWN_GLASS, ASSET_DOOR_BROWN_WIN,
    ASSET_DOOR_WHITE, ASSET_DOOR_WHITE_GLASS, ASSET_DOOR_WHITE_WIN,
    ASSET_FRAME_DOOR1, ASSET_FRAME_SIMPLE, ASSET_FRAME_TENT,
    ASSET_FRAME_WIN, ASSET_FRAME_WIN_WHITE,
    ASSET_WIN_SIMPLE, ASSET_WIN_SIMPLE_W, ASSET_WIN_DET, ASSET_WIN_DET_W,
    ASSET_WIN_TWIN_TENT, ASSET_WIN_TWIN_TENT_W,
    ASSET_WIN_TALL, ASSET_WIN_TALL_TOP,
    ASSET_WALL, ASSET_CORNER, ASSET_SIDEWALK, // NOTE: These are now BAKED, not instanced
    ASSET_PROP_TREE, ASSET_PROP_BENCH, ASSET_PROP_HYDRANT, ASSET_PROP_LIGHT,
    ASSET_PROP_PARK_TREE, 
    ASSET_COUNT
} AssetType;

typedef struct {
    AssetType window; AssetType windowTop; AssetType doorFrame; AssetType doorInner;
    AssetType balcony; bool hasAC; bool isSkyscraper; bool isWhiteTheme;
} BuildingStyle;

// Per-Chunk Storage (Only for dynamic instances now)
typedef struct {
    Matrix *transforms; 
    int count;
    int capacity;
} ChunkBatch;

#define BATCH_GROUPS 6 

typedef struct {
    ChunkBatch batches[BATCH_GROUPS][ASSET_COUNT];
    int *buildingIndices; // For physics
    int buildingCount;
    int buildingCapacity;
    Vector2 center;
} Chunk;

// [OPTIMIZATION] Static Geometry Sector
// Holds the merged mesh of Roads + Walls + Roofs + Sidewalks
typedef struct {
    Model staticModel; 
    bool hasGeometry;
} Sector;

// [OPTIMIZATION] Global Render Buffer (The Mega-Batch)
#define MAX_VISIBLE_INSTANCES 100000
typedef struct {
    Matrix *transforms;
    int count; 
} GlobalBatch;

typedef struct {
    Model models[ASSET_COUNT];       
    Color groupTints[BATCH_GROUPS][ASSET_COUNT];
    bool loaded;
    Texture2D whiteTex;
    
    Chunk *chunks;
    Sector *sectors;
    
    // Global buffers for single-draw-call rendering
    GlobalBatch globalBatches[BATCH_GROUPS][ASSET_COUNT];
    
    bool mapBaked;
    Shader instancingShader;
} CityRenderSystem;

static CityRenderSystem cityRenderer = {0};

Color cityPalette[] = {
    {152, 251, 152, 255}, {255, 182, 193, 255}, {255, 105, 97, 255},  
    {255, 200, 150, 255}, {200, 200, 200, 255}  
};

// --- HELPERS ---

int GetChunkIndex(Vector2 pos) {
    int x = (int)(pos.x / CHUNK_SIZE);
    int y = (int)(pos.y / CHUNK_SIZE);
    if (x < 0) x = 0; if (x >= GRID_WIDTH) x = GRID_WIDTH - 1;
    if (y < 0) y = 0; if (y >= GRID_WIDTH) y = GRID_WIDTH - 1;
    return y * GRID_WIDTH + x;
}

int GetSectorIndex(Vector2 pos) {
    int x = (int)(pos.x / SECTOR_SIZE);
    int y = (int)(pos.y / SECTOR_SIZE);
    if (x < 0) x = 0; if (x >= SECTOR_WIDTH) x = SECTOR_WIDTH - 1;
    if (y < 0) y = 0; if (y >= SECTOR_WIDTH) y = SECTOR_WIDTH - 1;
    return y * SECTOR_WIDTH + x;
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

// --- BAKING HELPERS ---

typedef struct {
    float *verts; float *cols;
    int count; int cap;
} SectorBuilder;

void PushSectorTri(SectorBuilder *sb, Vector3 v1, Vector3 v2, Vector3 v3, Color c) {
    if (sb->count + 3 >= sb->cap) {
        sb->cap = (sb->cap == 0) ? 4096 : sb->cap * 2;
        sb->verts = realloc(sb->verts, sb->cap * 3 * sizeof(float));
        sb->cols = realloc(sb->cols, sb->cap * 4 * sizeof(float));
    }
    float r=c.r/255.0f, g=c.g/255.0f, b=c.b/255.0f;
    
    int idx = sb->count * 3;
    sb->verts[idx++] = v1.x; sb->verts[idx++] = v1.y; sb->verts[idx++] = v1.z;
    sb->verts[idx++] = v2.x; sb->verts[idx++] = v2.y; sb->verts[idx++] = v2.z;
    sb->verts[idx++] = v3.x; sb->verts[idx++] = v3.y; sb->verts[idx++] = v3.z;
    
    int cIdx = sb->count * 4;
    for(int k=0; k<3; k++) {
        sb->cols[cIdx++] = r; sb->cols[cIdx++] = g; sb->cols[cIdx++] = b; sb->cols[cIdx++] = 1.0f;
    }
    sb->count += 3;
}

// [OPTIMIZATION] Bakes a rotated cube directly into the mesh (replaces Instancing for walls)
void BakeCubeToSector(SectorBuilder *sb, Vector3 pos, float rotDeg, Vector3 scale, Color color) {
    Vector3 pts[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f}, {0.5f, -0.5f,  0.5f}, {0.5f, 0.5f,  0.5f}, {-0.5f, 0.5f,  0.5f}
    };
    Matrix mat = MatrixMultiply(MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), MatrixRotateY(rotDeg*DEG2RAD)), MatrixTranslate(pos.x, pos.y, pos.z));
    for(int i=0; i<8; i++) pts[i] = Vector3Transform(pts[i], mat);
    
    // Front, Back, Top, Bottom, Left, Right faces
    PushSectorTri(sb, pts[0], pts[3], pts[1], color); PushSectorTri(sb, pts[1], pts[3], pts[2], color);
    PushSectorTri(sb, pts[5], pts[6], pts[4], color); PushSectorTri(sb, pts[4], pts[6], pts[7], color);
    PushSectorTri(sb, pts[3], pts[7], pts[2], color); PushSectorTri(sb, pts[2], pts[7], pts[6], color);
    PushSectorTri(sb, pts[1], pts[5], pts[0], color); PushSectorTri(sb, pts[0], pts[5], pts[4], color);
    PushSectorTri(sb, pts[4], pts[7], pts[0], color); PushSectorTri(sb, pts[0], pts[7], pts[3], color);
    PushSectorTri(sb, pts[1], pts[2], pts[5], color); PushSectorTri(sb, pts[5], pts[2], pts[6], color);
}

// --- TRIANGULATION ---

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

int TriangulatePolygon(Vector2 *points, int count, int *outIndices) {
    if (count < 3) return 0;
    int *indices = (int*)malloc(count * sizeof(int));
    for(int i=0; i<count; i++) indices[i] = i;
    
    if (GetPolygonSignedArea(points, count) > 0) {
        for(int i=0; i<count/2; i++) {
            int temp = indices[i]; indices[i] = indices[count-1-i]; indices[count-1-i] = temp;
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
            
            Vector2 a = points[prev]; Vector2 b = points[curr]; Vector2 c = points[next];
            
            if (IsValidEar(a, b, c, points, count, indices, activeCount)) {
                outIndices[triCount*3+0] = prev; outIndices[triCount*3+1] = curr; outIndices[triCount*3+2] = next;
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

// --- SHADERS & MODELS ---

static const char* INSTANCING_VSH = 
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in mat4 instanceTransform;\n"
    "out vec2 fragTexCoord; out vec4 fragColor; out vec3 fragNormal;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    fragTexCoord = vertexTexCoord; fragColor = vec4(1.0);\n"
    "    fragNormal = mat3(instanceTransform) * vertexNormal;\n"
    "    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* INSTANCING_FSH = 
    "#version 330\n"
    "in vec2 fragTexCoord; in vec4 fragColor; in vec3 fragNormal;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
    "const vec3 lightDir = normalize(vec3(0.5, 0.8, 0.3));\n" 
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    float lightDot = max(dot(normalize(fragNormal), lightDir), 0.0);\n"
    "    float light = 0.4 + (0.6 * lightDot);\n" 
    "    finalColor = texelColor * colDiffuse * fragColor * vec4(light, light, light, 1.0);\n"
    "}\n";

Model BakeMeshFromBuffers(float *vertices, float *colors, int triangleCount) {
    Mesh mesh = { 0 };
    mesh.triangleCount = triangleCount;
    mesh.vertexCount = triangleCount * 3;
    mesh.vertices = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.colors = (unsigned char *)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
    mesh.normals = (float *)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(mesh.vertexCount * 2 * sizeof(float));

    memcpy(mesh.vertices, vertices, mesh.vertexCount * 3 * sizeof(float));
    
    for (int i = 0; i < mesh.vertexCount; i++) {
        mesh.colors[i*4+0] = (unsigned char)(colors[i*4+0]*255);
        mesh.colors[i*4+1] = (unsigned char)(colors[i*4+1]*255);
        mesh.colors[i*4+2] = (unsigned char)(colors[i*4+2]*255);
        mesh.colors[i*4+3] = 255;
        mesh.normals[i*3+1] = 1.0f; // Simplified Up Normal
        mesh.texcoords[i*2] = 0.0f; 
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

// --- GRID LOGIC ---

void InitChunk(Chunk *c, int x, int y) {
    c->center = (Vector2){ (x * CHUNK_SIZE) + (CHUNK_SIZE/2.0f), (y * CHUNK_SIZE) + (CHUNK_SIZE/2.0f) };
    c->buildingIndices = NULL;
    c->buildingCount = 0;
    c->buildingCapacity = 0;
    
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int a=0; a<ASSET_COUNT; a++) {
            c->batches[g][a].count = 0;
            c->batches[g][a].capacity = 0; 
            c->batches[g][a].transforms = NULL;
        }
    }
}

void AddInstanceToGrid(int group, AssetType type, Vector3 pos, float rot, Vector3 scaleVec) {
    if (type >= ASSET_COUNT) return;
    int idx = GetChunkIndex((Vector2){pos.x, pos.z});
    Chunk *c = &cityRenderer.chunks[idx];
    ChunkBatch *b = &c->batches[group][type];

    if (b->count >= b->capacity) {
        b->capacity = (b->capacity == 0) ? 16 : b->capacity * 2;
        b->transforms = (Matrix*)realloc(b->transforms, b->capacity * sizeof(Matrix));
    }

    Matrix matScale = MatrixScale(scaleVec.x, scaleVec.y, scaleVec.z);
    Matrix matRot = MatrixRotateY(rot * DEG2RAD);
    Matrix matTrans = MatrixTranslate(pos.x, pos.y, pos.z);
    b->transforms[b->count] = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);
    b->count++;
}

void RegisterBuildingToGrid(int buildingIdx, Vector2 *footprint, int count) {
    Vector2 center = GetBuildingCenter(footprint, count);
    int idx = GetChunkIndex(center);
    Chunk *c = &cityRenderer.chunks[idx];
    if (c->buildingCount >= c->buildingCapacity) {
        c->buildingCapacity = (c->buildingCapacity == 0) ? 8 : c->buildingCapacity * 2;
        c->buildingIndices = (int*)realloc(c->buildingIndices, c->buildingCapacity * sizeof(int));
    }
    c->buildingIndices[c->buildingCount++] = buildingIdx;
}

// --- ASSET GENERATION ---

void GenerateParkFoliage(GameMap *map, MapArea *area) {
    if (area->pointCount < 3) return;
    float minX = FLT_MAX, maxX = -FLT_MAX, minY = FLT_MAX, maxY = -FLT_MAX;
    for(int i=0; i<area->pointCount; i++) {
        if(area->points[i].x < minX) minX = area->points[i].x;
        if(area->points[i].x > maxX) maxX = area->points[i].x;
        if(area->points[i].y < minY) minY = area->points[i].y;
        if(area->points[i].y > maxY) maxY = area->points[i].y;
    }
    int treeCount = (int)((maxX - minX) * (maxY - minY) / 150.0f);
    if(treeCount > 30) treeCount = 30; 
    
    for(int k=0; k<treeCount; k++) {
        float tx = GetRandomValue((int)minX, (int)maxX);
        float ty = GetRandomValue((int)minY, (int)maxY);
        Vector2 tPos = {tx, ty};
        if (CheckCollisionPointPoly(tPos, area->points, area->pointCount)) {
            Vector3 pos = {tPos.x, 0.0f, tPos.y};
            float rot = GetRandomValue(0, 360);
            AddInstanceToGrid(5, ASSET_PROP_PARK_TREE, pos, rot, (Vector3){ 1.5f, 4.0f, 1.5f });
            AddInstanceToGrid(5, ASSET_PROP_PARK_TREE, (Vector3){tPos.x, 3.5f, tPos.y}, rot, (Vector3){ 3.5f, 3.0f, 3.5f });
        }
    }
}

BuildingStyle GetBuildingStyle(Vector2 pos) {
    BuildingStyle style = {0};
    float distToCenter = Vector2Length(pos);
    bool isCenter = (distToCenter < REGION_CENTER_RADIUS);
    int roll = GetRandomValue(0, 100);
    if (isCenter && roll < 60) {
        style.isSkyscraper = true; style.window = ASSET_WIN_TALL; style.windowTop = ASSET_WIN_TALL_TOP;
        style.doorFrame = ASSET_FRAME_DOOR1; style.doorInner = ASSET_DOOR_BROWN_GLASS; style.balcony = ASSET_BALCONY;
    } else if (roll < 30) {
        style.window = ASSET_WIN_DET; style.windowTop = ASSET_WIN_DET; style.doorFrame = ASSET_FRAME_TENT; 
        style.doorInner = ASSET_DOOR_BROWN; style.balcony = ASSET_BALCONY; style.hasAC = true;
    } else if (roll < 70) {
        style.isWhiteTheme = true; style.window = ASSET_WIN_TWIN_TENT_W; style.windowTop = ASSET_WIN_TWIN_TENT_W; 
        style.doorFrame = ASSET_FRAME_WIN_WHITE; style.doorInner = ASSET_DOOR_WHITE_WIN; style.balcony = ASSET_BALCONY_WHITE; style.hasAC = true;
    } else {
        style.window = ASSET_WIN_SIMPLE; style.windowTop = ASSET_WIN_SIMPLE; style.doorFrame = ASSET_FRAME_SIMPLE; 
        style.doorInner = ASSET_DOOR_BROWN_WIN; style.balcony = ASSET_BALCONY; style.hasAC = true;
    }
    return style;
}

// [OPTIMIZATION] Bakes Walls/Corners/Beams into static mesh. Instances ONLY windows/doors/AC.
void BakeBuildingGeometry(Building *b, SectorBuilder *sectors) {
    float floorHeight = 3.0f * (MODEL_SCALE / 4.0f); 
    Vector2 bCenter = GetBuildingCenter(b->footprint, b->pointCount);
    BuildingStyle style = GetBuildingStyle(bCenter);
    if (style.isSkyscraper) floorHeight *= 0.85f; 
    int floors = (int)(b->height / floorHeight);
    if (style.isSkyscraper && floors < 6) floors = 6;
    else { if (floors < 2) floors = 2; if (floors > 5) floors = 5; }
    
    float visualHeight = floors * floorHeight;
    b->height = visualHeight; 
    int colorIdx = (style.isWhiteTheme) ? 5 : (abs((int)b->footprint[0].x) + abs((int)b->footprint[0].y)) % 5;
    Color wallColor = (colorIdx == 5) ? WHITE : cityPalette[colorIdx];
    float structuralDepth = MODEL_SCALE * MODEL_Z_SQUISH; 
    float cornerThick = structuralDepth * 0.85f; 

    for (int i = 0; i < b->pointCount; i++) {
        Vector2 p1 = b->footprint[i];
        Vector2 p2 = b->footprint[(i + 1) % b->pointCount];
        float dist = Vector2Distance(p1, p2);
        if (dist < 0.5f) continue;
        Vector2 dir = Vector2Normalize(Vector2Subtract(p2, p1));
        Vector2 wallNormal = { -dir.y, dir.x };
        float angle = atan2f(dir.y, dir.x) * RAD2DEG; 
        float modelRotation = -angle;
        Vector2 wallMid = Vector2Scale(Vector2Add(p1, p2), 0.5f);
        if (Vector2DotProduct(wallNormal, Vector2Subtract(bCenter, wallMid)) > 0) {
            wallNormal = Vector2Negate(wallNormal); modelRotation += 180.0f;                
        }
        
        // 1. Bake Corner
        Vector2 cornerInset = Vector2Scale(Vector2Normalize(Vector2Subtract(bCenter, p1)), 0.05f);
        Vector3 cornerPos = { p1.x + cornerInset.x, visualHeight/2.0f, p1.y + cornerInset.y };
        int sIdx = GetSectorIndex((Vector2){cornerPos.x, cornerPos.z});
        BakeCubeToSector(&sectors[sIdx], cornerPos, -angle, (Vector3){cornerThick, visualHeight, cornerThick}, wallColor);

        // 2. Loop Modules
        float moduleWidth = 2.0f * (MODEL_SCALE / 4.0f); 
        int modulesCount = (int)(dist / moduleWidth);
        float remainingSpace = dist - (modulesCount * moduleWidth);
        float startOffset = (remainingSpace / 2.0f) + (moduleWidth / 2.0f);
        Vector2 currentPos2D = Vector2Add(p1, Vector2Scale(dir, startOffset)); 
        float outwardOffset = 0.35f; 
        Vector3 floorBeamScale = { moduleWidth * 1.05f, 0.3f, structuralDepth * 0.25f };
        
        for (int m = 0; m < modulesCount; m++) {
            for (int f = 0; f < floors; f++) {
                float yPos = (f * floorHeight) + 0.1f;
                Vector3 pos = { currentPos2D.x + (wallNormal.x * outwardOffset), yPos, currentPos2D.y + (wallNormal.y * outwardOffset) };
                
                bool isDoor = (f == 0 && m == modulesCount / 2);
                if (isDoor) {
                    AddInstanceToGrid(colorIdx, style.doorFrame, pos, modelRotation, (Vector3){ MODEL_SCALE, MODEL_SCALE, structuralDepth });
                    AddInstanceToGrid(colorIdx, style.doorInner, pos, modelRotation, (Vector3){ MODEL_SCALE, MODEL_SCALE, structuralDepth * 0.8f });
                } else {
                    bool wantsBalcony = (!style.isSkyscraper && f > 0 && f < floors-1 && (m % 2 != 0) && GetRandomValue(0, 100) < 40);
                    if (wantsBalcony) AddInstanceToGrid(colorIdx, style.balcony, pos, modelRotation, (Vector3){ MODEL_SCALE, MODEL_SCALE, structuralDepth });
                    else {
                        AssetType winType = (f == floors - 1) ? style.windowTop : style.window;
                        AddInstanceToGrid(colorIdx, winType, pos, modelRotation, (Vector3){ MODEL_SCALE, MODEL_SCALE, structuralDepth });
                        if (style.hasAC && f < floors-1 && GetRandomValue(0, 100) < 15) {
                            AddInstanceToGrid(colorIdx, (GetRandomValue(0,1)?ASSET_AC_A:ASSET_AC_B), (Vector3){pos.x, pos.y-0.4f, pos.z}, modelRotation, (Vector3){ MODEL_SCALE, MODEL_SCALE, structuralDepth });
                        }
                    }
                }
                
                if (!style.isSkyscraper && f > 0) {
                    Vector3 beamPos = { pos.x, (f * floorHeight), pos.z };
                    BakeCubeToSector(&sectors[sIdx], beamPos, modelRotation, floorBeamScale, wallColor);
                }
                if (f == floors - 1) { // Cornice
                    Vector3 cornicePos = { currentPos2D.x + (wallNormal.x * 0.15f), visualHeight, currentPos2D.y + (wallNormal.y * 0.15f) };
                    BakeCubeToSector(&sectors[sIdx], cornicePos, modelRotation, (Vector3){ moduleWidth * 1.05f, 0.3f, structuralDepth }, wallColor);
                }
            }
            currentPos2D = Vector2Add(currentPos2D, Vector2Scale(dir, moduleWidth));
        }
        
        if (remainingSpace > 0.1f) {
            Vector2 f1 = Vector2Add(p1, Vector2Scale(dir, remainingSpace/4.0f));
            BakeCubeToSector(&sectors[sIdx], (Vector3){f1.x, visualHeight/2.0f, f1.y}, -angle, (Vector3){remainingSpace/2.0f, visualHeight, structuralDepth}, wallColor);
            Vector2 f2 = Vector2Add(p2, Vector2Scale(dir, -remainingSpace/4.0f));
            BakeCubeToSector(&sectors[sIdx], (Vector3){f2.x, visualHeight/2.0f, f2.y}, -angle, (Vector3){remainingSpace/2.0f, visualHeight, structuralDepth}, wallColor);
        }
    }
}

// [OPTIMIZATION] Bakes sidewalks into static mesh. Instances props.
void BakeRoadDetails(GameMap *map, SectorBuilder *sectors) {
    float sidewalkW = 2.5f; float propSpacing = 15.0f; 
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
        
        for(int side=-1; side<=1; side+=2) {
            Vector2 swStart = Vector2Add(Vector2Add(s, Vector2Scale(right, offsetDist * side)), Vector2Scale(dir, 0));
            float swLen = len;
            if (swLen < 0.1f) continue;
            Vector2 swMid = Vector2Add(swStart, Vector2Scale(dir, swLen/2.0f));
            int sIdx = GetSectorIndex(swMid);
            
            // BAKE SIDEWALK CUBE
            BakeCubeToSector(&sectors[sIdx], (Vector3){ swMid.x, 0.10f, swMid.y }, -angle, (Vector3){swLen, 0.10f, sidewalkW}, (Color){180, 180, 180, 255});

            // INSTANCE PROPS
            int propCount = (int)(swLen / propSpacing);
            for(int p=0; p<propCount; p++) {
                float distAlong = (p * propSpacing) + (propSpacing * 0.5f);
                Vector2 propPos2D = Vector2Add(swStart, Vector2Scale(dir, distAlong));
                
                bool blocked = false;
                for(int L=0; L<map->locationCount; L++) if(Vector2Distance(propPos2D, map->locations[L].position) < 8.0f) blocked = true;
                if (blocked) continue;

                Vector3 propPos = { propPos2D.x, 0.2f, propPos2D.y };
                float rot = (side == 1) ? -angle : -angle + 180.0f; 
                int roll = GetRandomValue(0, 100);
                if (roll < 40) { 
                      AddInstanceToGrid(5, ASSET_PROP_TREE, propPos, rot, (Vector3){ 0.8f, 3.5f, 0.8f });
                      AddInstanceToGrid(5, ASSET_PROP_TREE, (Vector3){ propPos.x, 3.0f, propPos.z }, rot, (Vector3){ 2.5f, 2.0f, 2.5f });
                } else if (roll < 60) AddInstanceToGrid(5, ASSET_PROP_BENCH, propPos, rot, (Vector3){ 2.0f, 0.5f, 0.8f });
                else if (roll < 70) AddInstanceToGrid(5, ASSET_PROP_HYDRANT, propPos, rot, (Vector3){ 0.5f, 0.8f, 0.5f });
                else if (roll < 85) AddInstanceToGrid(5, ASSET_PROP_LIGHT, propPos, rot, (Vector3){ 0.3f, 5.0f, 0.3f });
            }
        }
    }
}

// [OPTIMIZATION] Main baking entry point
void BakeMapElements(GameMap *map) {
    if (cityRenderer.mapBaked) return;
    printf("Baking Massive Static Sectors...\n");

    int sectorCount = SECTOR_WIDTH * SECTOR_WIDTH;
    SectorBuilder *sectors = calloc(sectorCount, sizeof(SectorBuilder));
    int *tempIndices = (int*)malloc(1024 * sizeof(int));
    
    // 1. Bake Roads
    for (int i = 0; i < map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;
        int sIdx = GetSectorIndex(s);
        
        float width = (e.width * MAP_SCALE) * 2.0f;
        Vector3 start = {s.x, 0.02f, s.y}; Vector3 end = {en.x, 0.02f, en.y};
        Vector3 diff = Vector3Subtract(end, start);
        if (Vector3Length(diff) < 0.1f) continue;
        Vector3 right = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, diff));
        Vector3 halfW = Vector3Scale(right, width * 0.5f);
        
        PushSectorTri(&sectors[sIdx], Vector3Subtract(start, halfW), Vector3Add(end, halfW), Vector3Add(start, halfW), (Color){40,40,40,255});
        PushSectorTri(&sectors[sIdx], Vector3Subtract(start, halfW), Vector3Subtract(end, halfW), Vector3Add(end, halfW), (Color){40,40,40,255});
    }

    // 2. Bake Buildings (Walls & Roofs)
    for (int i = 0; i < map->buildingCount; i++) {
        RegisterBuildingToGrid(i, map->buildings[i].footprint, map->buildings[i].pointCount);
        BakeBuildingGeometry(&map->buildings[i], sectors);
        
        // Roofs
        Vector2 bCenter = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        int sIdx = GetSectorIndex(bCenter);
        float h = map->buildings[i].height;
        int triCount = TriangulatePolygon(map->buildings[i].footprint, map->buildings[i].pointCount, tempIndices);
        for(int k=0; k<triCount; k++) {
            Vector2 p1 = map->buildings[i].footprint[tempIndices[k*3]];
            Vector2 p2 = map->buildings[i].footprint[tempIndices[k*3+1]];
            Vector2 p3 = map->buildings[i].footprint[tempIndices[k*3+2]];
            PushSectorTri(&sectors[sIdx], (Vector3){p1.x, h, p1.y}, (Vector3){p2.x, h, p2.y}, (Vector3){p3.x, h, p3.y}, (Color){80,80,90,255});
        }
    }

    // 3. Bake Areas/Foliage
    for (int i = 0; i < map->areaCount; i++) {
        if(map->areas[i].pointCount < 3) continue;
        Color col = map->areas[i].color;
        if (col.g > col.r && col.g > col.b) { col = COLOR_PARK; GenerateParkFoliage(map, &map->areas[i]); }
        
        Vector2 center = {0};
        for(int j=0; j<map->areas[i].pointCount; j++) center = Vector2Add(center, map->areas[i].points[j]);
        center = Vector2Scale(center, 1.0f/map->areas[i].pointCount);
        int sIdx = GetSectorIndex(center);
        
        for(int j=0; j<map->areas[i].pointCount; j++) {
            Vector2 p1 = map->areas[i].points[j];
            Vector2 p2 = map->areas[i].points[(j+1)%map->areas[i].pointCount];
            PushSectorTri(&sectors[sIdx], (Vector3){center.x, 0.01f, center.y}, 
                         (Vector3){p2.x, 0.01f, p2.y}, (Vector3){p1.x, 0.01f, p1.y}, col);
        }
    }

    // 4. Bake Sidewalks
    BakeRoadDetails(map, sectors);

    // Finalize
    for(int i=0; i<sectorCount; i++) {
        cityRenderer.sectors[i].hasGeometry = false;
        if (sectors[i].count > 0) {
            cityRenderer.sectors[i].staticModel = BakeMeshFromBuffers(sectors[i].verts, sectors[i].cols, sectors[i].count/3);
            cityRenderer.sectors[i].hasGeometry = true;
        }
        if(sectors[i].verts) free(sectors[i].verts);
        if(sectors[i].cols) free(sectors[i].cols);
    }
    free(sectors); free(tempIndices);
    cityRenderer.mapBaked = true;
}

// --- INIT & LOAD ---

void LoadCityAssets() {
    if (cityRenderer.loaded) return;
    
    cityRenderer.chunks = calloc(GRID_WIDTH*GRID_WIDTH, sizeof(Chunk));
    for(int y=0; y<GRID_WIDTH; y++) 
        for(int x=0; x<GRID_WIDTH; x++) InitChunk(&cityRenderer.chunks[y*GRID_WIDTH+x], x, y);
    
    cityRenderer.sectors = calloc(SECTOR_WIDTH*SECTOR_WIDTH, sizeof(Sector));

    // [OPTIMIZATION] Allocate Mega-Batches
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int m=0; m<ASSET_COUNT; m++) {
            cityRenderer.globalBatches[g][m].transforms = malloc(MAX_VISIBLE_INSTANCES * sizeof(Matrix));
            cityRenderer.globalBatches[g][m].count = 0;
        }
    }

    cityRenderer.instancingShader = LoadShaderFromMemory(INSTANCING_VSH, INSTANCING_FSH);
    cityRenderer.instancingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(cityRenderer.instancingShader, "instanceTransform");

    Image whiteImg = GenImageColor(1, 1, WHITE);
    cityRenderer.whiteTex = LoadTextureFromImage(whiteImg);
    UnloadImage(whiteImg);

    #define LOAD_ASSET(enumIdx, filename) \
        { \
            char fullPath[256]; sprintf(fullPath, "resources/Buildings/%s", filename); \
            cityRenderer.models[enumIdx] = LoadModel(fullPath); \
            if (cityRenderer.models[enumIdx].meshCount == 0) cityRenderer.models[enumIdx] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); \
            else { \
                for(int i=0; i<cityRenderer.models[enumIdx].materialCount; i++) { \
                    cityRenderer.models[enumIdx].materials[i].shader = cityRenderer.instancingShader; \
                    if(cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) \
                       GenTextureMipmaps(&cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture); \
                } \
            } \
        }

    // Load OBJs
    LOAD_ASSET(ASSET_AC_A, "detail-ac-a.obj");
    LOAD_ASSET(ASSET_AC_B, "detail-ac-b.obj");
    LOAD_ASSET(ASSET_BALCONY, "balcony.obj");
    LOAD_ASSET(ASSET_BALCONY_WHITE, "balcony_white.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN, "door-brown.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_GLASS, "door-brown-glass.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_WIN, "door-brown-window.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE, "door-white.obj"); 
    LOAD_ASSET(ASSET_DOOR_WHITE_GLASS, "door-white-glass.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE_WIN, "door-white-window.obj");
    LOAD_ASSET(ASSET_FRAME_DOOR1, "door1.obj");
    LOAD_ASSET(ASSET_FRAME_SIMPLE, "simple_door.obj");
    LOAD_ASSET(ASSET_FRAME_TENT, "doorframe_glass_tent.obj");
    LOAD_ASSET(ASSET_FRAME_WIN, "window_door.obj");
    LOAD_ASSET(ASSET_FRAME_WIN_WHITE, "window_door_white.obj");
    LOAD_ASSET(ASSET_WIN_SIMPLE, "Windows_simple.obj");
    LOAD_ASSET(ASSET_WIN_SIMPLE_W, "Windows_simple_white.obj");
    LOAD_ASSET(ASSET_WIN_DET, "Windows_detailed.obj");
    LOAD_ASSET(ASSET_WIN_DET_W, "Windows_detailed_white.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT, "Twin_window_tents.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT_W, "Twin_window_tents_white.obj");
    LOAD_ASSET(ASSET_WIN_TALL, "windows_tall.obj");
    LOAD_ASSET(ASSET_WIN_TALL_TOP, "windows_tall_top.obj");

    // Procedural Placeholders (Only for Prop instances now)
    Model cubeModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Material propMat = LoadMaterialDefault();
    propMat.shader = cityRenderer.instancingShader;
    propMat.maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex;
    cubeModel.materials[0] = propMat;

    cityRenderer.models[ASSET_PROP_TREE] = cubeModel;
    cityRenderer.models[ASSET_PROP_BENCH] = cubeModel;
    cityRenderer.models[ASSET_PROP_HYDRANT] = cubeModel;
    cityRenderer.models[ASSET_PROP_LIGHT] = cubeModel;
    cityRenderer.models[ASSET_PROP_PARK_TREE] = cubeModel;

    // Tints
    for (int i = 0; i < 5; i++) for (int m = 0; m < ASSET_COUNT; m++) cityRenderer.groupTints[i][m] = cityPalette[i];
    for (int m = 0; m < ASSET_COUNT; m++) cityRenderer.groupTints[5][m] = WHITE;
    
    cityRenderer.groupTints[5][ASSET_PROP_TREE] = (Color){30, 100, 30, 255};      
    cityRenderer.groupTints[5][ASSET_PROP_BENCH] = (Color){100, 70, 40, 255};     
    cityRenderer.groupTints[5][ASSET_PROP_HYDRANT] = (Color){200, 40, 40, 255};   
    cityRenderer.groupTints[5][ASSET_PROP_LIGHT] = (Color){80, 80, 90, 255};      
    cityRenderer.groupTints[5][ASSET_PROP_PARK_TREE] = (Color){10, 90, 20, 255};  

    cityRenderer.loaded = true;
}

// --- VISUAL HELPERS ---

void DrawZoneMarker(Vector3 pos, Color color) {
    float time = GetTime();
    float scale = 1.0f + sinf(time * 3.0f) * 0.1f;
    float height = 4.0f;
    float radius = 4.0f * scale;
    DrawCylinder(pos, radius, radius, height, 16, Fade(color, 0.3f));
    DrawCylinderWires(pos, radius, radius, height, 16, color);
    Vector3 ringPos = { pos.x, pos.y + height + 0.5f + sinf(time * 2.0f) * 0.5f, pos.z };
    DrawCircle3D(ringPos, radius * 0.8f, (Vector3){1,0,0}, 90.0f, color);
}

static void DrawCenteredLabel(Camera camera, Vector3 position, const char *text, Color color) {
    Vector2 screenPos = GetWorldToScreen(position, camera);
    if (screenPos.x > 0 && screenPos.x < GetScreenWidth() && 
        screenPos.y > 0 && screenPos.y < GetScreenHeight()) {
        int fontSize = 20;
        int textW = MeasureText(text, fontSize);
        DrawRectangle(screenPos.x - textW/2 - 4, screenPos.y - fontSize/2 - 4, textW + 8, fontSize + 8, BLACK);
        DrawText(text, (int)screenPos.x - textW/2, (int)screenPos.y - fontSize/2, fontSize, WHITE);
    }
}

// --- RENDER PIPELINE ---

void DrawGameMap(GameMap *map, Camera camera) {
    rlDisableBackfaceCulling();
    Vector2 pPos = {camera.position.x, camera.position.z};
    
    // 0. Floor
    DrawPlane((Vector3){0, -0.05f, 0}, (Vector2){10000.0f, 10000.0f}, (Color){80, 80, 80, 255});

    if (!cityRenderer.mapBaked) return;

    // 1. Draw Static Sectors (Contains ROADS, WALLS, SIDEWALKS)
    int pSecIdx = GetSectorIndex(pPos);
    int sx = pSecIdx % SECTOR_WIDTH; int sy = pSecIdx / SECTOR_WIDTH;
    
    // Draw 3x3 sectors (covers immediate area)
    for(int y=-1; y<=1; y++) {
        for(int x=-1; x<=1; x++) {
            int tx = sx+x; int ty = sy+y;
            if(tx>=0 && tx<SECTOR_WIDTH && ty>=0 && ty<SECTOR_WIDTH) {
                Sector *s = &cityRenderer.sectors[ty*SECTOR_WIDTH+tx];
                if(s->hasGeometry) DrawModel(s->staticModel, (Vector3){0,0,0}, 1.0f, WHITE);
            }
        }
    }

    // 2. Clear Mega-Batch Buffers
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int m=0; m<ASSET_COUNT; m++) cityRenderer.globalBatches[g][m].count = 0;
    }

    // 3. Cull Chunks & Aggregate Instances
    int pChunkIdx = GetChunkIndex(pPos);
    int cx = pChunkIdx % GRID_WIDTH; int cy = pChunkIdx / GRID_WIDTH;
    int range = 6; 
    
    for(int y=-range; y<=range; y++) {
        for(int x=-range; x<=range; x++) {
            int tx = cx+x; int ty = cy+y;
            if(tx<0 || tx>=GRID_WIDTH || ty<0 || ty>=GRID_WIDTH) continue;
            
            Chunk *c = &cityRenderer.chunks[ty*GRID_WIDTH+tx];
            float dist = Vector2Distance(pPos, c->center);
            
            // Only process instances (Windows/Props) if relatively close
            if (dist > LOD_DIST_WINDOWS) continue; 
            
            // Append this chunk's instances to global batch
            for(int g=0; g<BATCH_GROUPS; g++) {
                for(int m=0; m<ASSET_COUNT; m++) {
                    ChunkBatch *b = &c->batches[g][m];
                    if (b->count == 0) continue;
                    
                    GlobalBatch *gb = &cityRenderer.globalBatches[g][m];
                    if (gb->count + b->count > MAX_VISIBLE_INSTANCES) continue; // safety
                    
                    memcpy(gb->transforms + gb->count, b->transforms, b->count * sizeof(Matrix));
                    gb->count += b->count;
                }
            }
        }
    }

    // 4. DRAW Mega-Batches (Reduced Draw Calls)
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int m=0; m<ASSET_COUNT; m++) {
            GlobalBatch *gb = &cityRenderer.globalBatches[g][m];
            if (gb->count > 0) {
                cityRenderer.models[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = cityRenderer.groupTints[g][m];
                DrawMeshInstanced(cityRenderer.models[m].meshes[0], cityRenderer.models[m].materials[0], gb->transforms, gb->count);
            }
        }
    }

    // 5. Locations
    for (int i = 0; i < map->locationCount; i++) {
        if (Vector2Distance(pPos, map->locations[i].position) > LOD_DIST_PROPS) continue;
        Vector3 pos = { map->locations[i].position.x, 1.0f, map->locations[i].position.y };
        Color c = RED;
        if (map->locations[i].type == LOC_FUEL) {
            c = ORANGE;
            Vector3 pumpPos = { pos.x + 2.0f, 1.0f, pos.z + 2.0f };
            DrawCube(pumpPos, 1.0f, 2.0f, 1.0f, YELLOW);
            DrawCubeWires(pumpPos, 1.0f, 2.0f, 1.0f, BLACK);
        }
        else if (map->locations[i].type == LOC_MECHANIC) {
            c = BLUE;
            Vector3 mechPos = { pos.x + 2.0f, 0.5f, pos.z + 2.0f }; 
            DrawCube(mechPos, 1.5f, 1.0f, 1.5f, BLUE); 
            DrawCubeWires(mechPos, 1.5f, 1.0f, 1.5f, BLACK);
        }
        else if (map->locations[i].type == LOC_FOOD) c = GREEN;
        
        DrawSphere(pos, 1.5f, Fade(c, 0.8f));
        DrawLine3D(pos, (Vector3){pos.x, 4.0f, pos.z}, BLACK);
    }
    
    // 6. Events
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            Vector3 pos = { map->events[i].position.x, 1.5f, map->events[i].position.y };
            DrawCube(pos, 3.0f, 3.0f, 3.0f, ORANGE);
            DrawCubeWires(pos, 3.1f, 3.1f, 3.1f, BLACK);
        }
    }
    
    rlEnableBackfaceCulling();
    EndMode3D();
    
    // 7. Labels
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            Vector3 pos = { map->events[i].position.x, 1.5f, map->events[i].position.y };
            DrawCenteredLabel(camera, pos, map->events[i].label, WHITE);
        }
    }
    for (int i = 0; i < map->locationCount; i++) {
        if ((map->locations[i].type == LOC_FUEL || map->locations[i].type == LOC_MECHANIC) && 
            Vector2Distance(pPos, map->locations[i].position) < 15.0f) {
            Vector3 targetPos = { map->locations[i].position.x + 2.0f, 2.5f, map->locations[i].position.y + 2.0f };
            const char* txt = (map->locations[i].type == LOC_FUEL) ? "Refuel [E]" : "Mechanic [E]";
            DrawCenteredLabel(camera, targetPos, txt, YELLOW);
        }
    }
    BeginMode3D(camera);
}

// --- LOGIC ---

void ClearEvents(GameMap *map) {
    for (int i = 0; i < MAX_EVENTS; i++) {
        map->events[i].active = false; map->events[i].timer = 0;
    }
}

void TriggerSpecificEvent(GameMap *map, MapEventType type, Vector3 playerPos, Vector3 playerFwd) {
    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!map->events[i].active) { slot = i; break; }
    }
    if (slot == -1) return;
    Vector2 spawnPos = { playerPos.x + (playerFwd.x * 15.0f), playerPos.z + (playerFwd.z * 15.0f) };
    map->events[slot].active = true;
    map->events[slot].type = type;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f; 
    map->events[slot].timer = 30.0f; 
    if (type == EVENT_CRASH) snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    else if (type == EVENT_ROADWORK) snprintf(map->events[slot].label, 64, "ROAD WORK");
}

void TriggerRandomEvent(GameMap *map, Vector3 playerPos, Vector3 playerFwd) {
    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) { if (!map->events[i].active) { slot = i; break; } }
    if (slot == -1) return; 
    int attempts = 0; bool found = false; Vector2 spawnPos = {0};
    while (attempts < 50) {
        int eIdx = GetRandomValue(0, map->edgeCount - 1);
        Edge e = map->edges[eIdx];
        Vector2 p1 = map->nodes[e.startNode].position;
        Vector2 p2 = map->nodes[e.endNode].position;
        Vector2 mid = Vector2Scale(Vector2Add(p1, p2), 0.5f); 
        float dist = Vector2Distance(mid, (Vector2){playerPos.x, playerPos.z});
        if (dist > 100.0f && dist < 500.0f) { spawnPos = mid; found = true; break; }
        attempts++;
    }
    if (!found) return; 
    map->events[slot].active = true;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f;
    map->events[slot].timer = 120.0f; 
    if (GetRandomValue(0, 100) < 50) {
        map->events[slot].type = EVENT_CRASH; snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    } else {
        map->events[slot].type = EVENT_ROADWORK; snprintf(map->events[slot].label, 64, "ROAD WORK");
    }
}

void UpdateDevControls(GameMap *map, Vector3 playerPos, Vector3 playerFwd) {
    if (IsKeyPressed(KEY_F1)) TriggerSpecificEvent(map, EVENT_CRASH, playerPos, playerFwd);
    if (IsKeyPressed(KEY_F2)) TriggerSpecificEvent(map, EVENT_ROADWORK, playerPos, playerFwd);
    if (IsKeyPressed(KEY_F4)) ClearEvents(map);
}

void UpdateMapEffects(GameMap *map, Vector3 playerPos) {
    for(int i=0; i<MAX_EVENTS; i++) {
        if(map->events[i].active) {
            map->events[i].timer -= GetFrameTime();
            if(map->events[i].timer <= 0) map->events[i].active = false;
        }
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

void BuildMapGraph(GameMap *map) {
    map->graph = (NodeGraph*)calloc(map->nodeCount, sizeof(NodeGraph));
    for (int i = 0; i < map->edgeCount; i++) {
        int u = map->edges[i].startNode; int v = map->edges[i].endNode;
        if (u >= map->nodeCount || v >= map->nodeCount) continue;
        float dist = Vector2Distance(map->nodes[u].position, map->nodes[v].position);
        if (map->graph[u].count >= map->graph[u].capacity) {
            map->graph[u].capacity = (map->graph[u].capacity == 0) ? 4 : map->graph[u].capacity * 2;
            map->graph[u].connections = realloc(map->graph[u].connections, map->graph[u].capacity * sizeof(GraphConnection));
        }
        map->graph[u].connections[map->graph[u].count++] = (GraphConnection){v, dist};
        if (map->graph[v].count >= map->graph[v].capacity) {
            map->graph[v].capacity = (map->graph[v].capacity == 0) ? 4 : map->graph[v].capacity * 2;
            map->graph[v].connections = realloc(map->graph[v].connections, map->graph[v].capacity * sizeof(GraphConnection));
        }
        map->graph[v].connections[map->graph[v].count++] = (GraphConnection){u, dist};
    }
}

int GetClosestNode(GameMap *map, Vector2 position) {
    int bestNode = -1; float minDst = FLT_MAX;
    for (int i = 0; i < map->nodeCount; i++) {
        if (map->graph && map->graph[i].count == 0) continue; 
        if (fabsf(position.x - map->nodes[i].position.x) > 100.0f) continue;
        if (fabsf(position.y - map->nodes[i].position.y) > 100.0f) continue;
        float d = Vector2DistanceSqr(position, map->nodes[i].position);
        if (d < minDst) { minDst = d; bestNode = i; }
    }
    return bestNode;
}

int FindPath(GameMap *map, Vector2 startPos, Vector2 endPos, Vector2 *outPath, int maxPathLen) {
    if (!map->graph) BuildMapGraph(map);
    int startNode = GetClosestNode(map, startPos);
    int endNode = GetClosestNode(map, endPos);
    if (startNode == -1 || endNode == -1 || startNode == endNode) return 0;
    
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

// --- O(1) Collision Check using Grid ---
bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    Vector2 p = { x, z };
    int idx = GetChunkIndex(p);
    int cx = idx % GRID_WIDTH;
    int cy = idx / GRID_WIDTH;

    // Check current chunk and 8 neighbors
    for(int y=-1; y<=1; y++) {
        for(int x_off=-1; x_off<=1; x_off++) {
            int tx = cx + x_off; int ty = cy + y;
            if (tx < 0 || tx >= GRID_WIDTH || ty < 0 || ty >= GRID_WIDTH) continue;
            
            Chunk *c = &cityRenderer.chunks[ty * GRID_WIDTH + tx];
            for(int i=0; i<c->buildingCount; i++) {
                Building *b = &map->buildings[c->buildingIndices[i]];
                if (CheckCollisionPointPoly(p, b->footprint, b->pointCount)) return true;
            }
        }
    }
    
    // Check active events
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            if (Vector2Distance(p, map->events[i].position) < (map->events[i].radius + radius)) return true;
        }
    }
    return false;
}

// --- CLEANUP ---

void UnloadGameMap(GameMap *map) {
    if (cityRenderer.loaded) {
        UnloadTexture(cityRenderer.whiteTex);
        // Free Global Batches
        for(int g=0; g<BATCH_GROUPS; g++) {
            for(int m=0; m<ASSET_COUNT; m++) if(cityRenderer.globalBatches[g][m].transforms) free(cityRenderer.globalBatches[g][m].transforms);
        }
        // Free Chunks
        for(int i=0; i<GRID_WIDTH*GRID_WIDTH; i++) {
            Chunk *c = &cityRenderer.chunks[i];
            if(c->buildingIndices) free(c->buildingIndices);
            for(int g=0; g<BATCH_GROUPS; g++) 
                for(int m=0; m<ASSET_COUNT; m++) if(c->batches[g][m].transforms) free(c->batches[g][m].transforms);
        }
        free(cityRenderer.chunks);
        // Free Sectors
        for(int i=0; i<SECTOR_WIDTH*SECTOR_WIDTH; i++) {
            if(cityRenderer.sectors[i].hasGeometry) UnloadModel(cityRenderer.sectors[i].staticModel);
        }
        free(cityRenderer.sectors);
        
        // Free Models (Skip ASSET_WALL, ASSET_CORNER etc because they weren't loaded as files)
        // Only loadable assets
        for (int m = 0; m < ASSET_PROP_TREE; m++) {
             // Only unload if it was a loaded model (checking vertex count > 0 is a rough heuristic, 
             // but simpler: check index against enum)
             if (m < ASSET_WALL) UnloadModel(cityRenderer.models[m]);
        }
        UnloadModel(cityRenderer.models[ASSET_PROP_TREE]); // Shared procedural model
        
        cityRenderer.loaded = false;
    }
    if(map->nodes) free(map->nodes);
    if(map->edges) free(map->edges);
    if(map->buildingCount > 0) for (int i = 0; i < map->buildingCount; i++) free(map->buildings[i].footprint);
    if(map->areaCount > 0) for (int i = 0; i < map->areaCount; i++) free(map->areas[i].points);
    if (map->buildings) free(map->buildings);
    if (map->locations) free(map->locations);
    if (map->areas) free(map->areas);
    if (map->graph) {
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }
}

// --- FILE PARSING ---

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));
    map.areas = (MapArea *)calloc(MAX_AREAS, sizeof(MapArea));
    map.isBatchLoaded = false;
    
    ClearEvents(&map);
    LoadCityAssets(); 

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
                float h; int r, g, b; char *ptr = line; int read = 0;
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
                int type, r, g, b; char *ptr = line; int read = 0;
                MapArea *area = &map.areas[map.areaCount];
                if (sscanf(ptr, "%d %d %d %d%n", &type, &r, &g, &b, &read) == 4) {
                    area->type = type; area->color = (Color){r, g, b, 255}; ptr += read;
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

    // Call the NEW baking logic
    BakeMapElements(&map); 

    return map;
}
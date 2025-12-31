#include "map.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

// --- CONFIGURATION ---
const float MAP_SCALE = 0.4f;
const float RENDER_DIST_BASE = 500.0f;  
const float RENDER_DIST_FWD  = 1600.0f; 

#define MODEL_SCALE 1.8f         
#define MODEL_Z_SQUISH 0.4f      
#define MAX_INSTANCES 40000 
#define USE_INSTANCING 1         
#define REGION_CENTER_RADIUS 600.0f 

typedef enum {
    ASSET_AC_A = 0, ASSET_AC_B, ASSET_BALCONY, ASSET_BALCONY_WHITE,
    ASSET_DOOR_BROWN, ASSET_DOOR_BROWN_GLASS, ASSET_DOOR_BROWN_WIN,
    ASSET_DOOR_WHITE, ASSET_DOOR_WHITE_GLASS, ASSET_DOOR_WHITE_WIN,
    ASSET_FRAME_DOOR1, ASSET_FRAME_SIMPLE, ASSET_FRAME_TENT,
    ASSET_FRAME_WIN, ASSET_FRAME_WIN_WHITE,
    ASSET_WIN_SIMPLE, ASSET_WIN_SIMPLE_W, ASSET_WIN_DET, ASSET_WIN_DET_W,
    ASSET_WIN_TWIN_TENT, ASSET_WIN_TWIN_TENT_W,
    ASSET_WIN_TALL, ASSET_WIN_TALL_TOP,
    ASSET_WALL, ASSET_CORNER,
    ASSET_SIDEWALK,
    ASSET_PROP_TREE, ASSET_PROP_BENCH, ASSET_PROP_HYDRANT, ASSET_PROP_LIGHT,
    ASSET_PROP_PARK_TREE, 
    ASSET_COUNT
} AssetType;

typedef struct {
    AssetType window; AssetType windowTop; AssetType doorFrame; AssetType doorInner;
    AssetType balcony; bool hasAC; bool isSkyscraper; bool isWhiteTheme;
} BuildingStyle;

typedef struct {
    Matrix *transforms; int count; Color tint;
} RenderBatch;

#define BATCH_GROUPS 6 

typedef struct {
    Model models[ASSET_COUNT];       
    RenderBatch batches[BATCH_GROUPS][ASSET_COUNT]; 
    bool loaded;
    Texture2D whiteTex;
    Model roadModel;
    Model markingsModel;
    Model areaModel;
    Model roofModel;
    bool mapBaked;
} CityRenderSystem;

static CityRenderSystem cityRenderer = {0};

Color cityPalette[] = {
    {152, 251, 152, 255}, {255, 182, 193, 255}, {255, 105, 97, 255},  
    {255, 200, 150, 255}, {200, 200, 200, 255}  
};

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

void InitRenderBatch(RenderBatch *batch, Color color) {
    batch->transforms = (Matrix *)malloc(MAX_INSTANCES * sizeof(Matrix));
    batch->count = 0;
    batch->tint = color;
}

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

Model BakeStaticGeometry(float *vertices, float *colors, int triangleCount) {
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
    for (int i = 0; i < mesh.vertexCount; i++) {
        mesh.normals[i*3 + 0] = 0.0f;
        mesh.normals[i*3 + 1] = 1.0f;
        mesh.normals[i*3 + 2] = 0.0f;
        mesh.texcoords[i*2] = 0.0f; mesh.texcoords[i*2+1] = 0.0f;
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

#define ADD_INSTANCE(group, assetType, pos, rotation, scaleVec) \
    if (cityRenderer.batches[group][assetType].count < MAX_INSTANCES) { \
        Matrix matScale = MatrixScale(scaleVec.x, scaleVec.y, scaleVec.z); \
        Matrix matRot = MatrixRotateY(rotation * DEG2RAD); \
        Matrix matTrans = MatrixTranslate(pos.x, pos.y, pos.z); \
        Matrix final = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans); \
        cityRenderer.batches[group][assetType].transforms[cityRenderer.batches[group][assetType].count++] = final; \
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
    int treeCount = (int)((areaW * areaH) / 150.0f);
    if(treeCount > 30) treeCount = 30; 
    
    for(int k=0; k<treeCount; k++) {
        float tx = GetRandomValue((int)minX, (int)maxX);
        float ty = GetRandomValue((int)minY, (int)maxY);
        Vector2 tPos = {tx, ty};
        
        if (CheckCollisionPointPoly(tPos, area->points, area->pointCount)) {
            Vector3 pos = {tPos.x, 0.0f, tPos.y};
            float rot = GetRandomValue(0, 360);
            Vector3 scale = { 1.5f, 4.0f, 1.5f }; 
            ADD_INSTANCE(5, ASSET_PROP_PARK_TREE, pos, rot, scale);
            Vector3 lPos = {tPos.x, 3.5f, tPos.y};
            Vector3 lScale = { 3.5f, 3.0f, 3.5f };
            ADD_INSTANCE(5, ASSET_PROP_PARK_TREE, lPos, rot, lScale);
        }
    }
}

void BakeMapElements(GameMap *map) {
    if (cityRenderer.mapBaked) return;
    printf("Baking Map Geometry...\n");

    int maxRoadTris = map->edgeCount * 2 + 1000;
    int maxMarkTris = map->edgeCount * 2 + 1000;
    float *roadVerts = (float*)malloc(maxRoadTris * 3 * 3 * sizeof(float));
    float *markVerts = (float*)malloc(maxMarkTris * 3 * 3 * sizeof(float));
    int rVCount = 0;
    int mVCount = 0;

    for (int i = 0; i < map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        
        Vector2 s = map->nodes[e.startNode].position;
        Vector2 en = map->nodes[e.endNode].position;
        float finalWidth = (e.width * MAP_SCALE) * 2.0f;
        Vector3 start = {s.x, 0.02f, s.y}; 
        Vector3 end = {en.x, 0.02f, en.y};
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
            Vector3 mStart = { start.x, 0.035f, start.z }; 
            Vector3 mEnd = { end.x, 0.035f, end.z };
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

    cityRenderer.roadModel = BakeStaticGeometry(roadVerts, NULL, rVCount / 3);
    
    for(int i=0; i<cityRenderer.roadModel.meshCount; i++) {
        for(int j=0; j<cityRenderer.roadModel.meshes[i].vertexCount*4; j+=4) {
            cityRenderer.roadModel.meshes[i].colors[j] = COLOR_ROAD.r;
            cityRenderer.roadModel.meshes[i].colors[j+1] = COLOR_ROAD.g;
            cityRenderer.roadModel.meshes[i].colors[j+2] = COLOR_ROAD.b;
            cityRenderer.roadModel.meshes[i].colors[j+3] = COLOR_ROAD.a;
        }
    }
    UploadMesh(&cityRenderer.roadModel.meshes[0], false);

    cityRenderer.markingsModel = BakeStaticGeometry(markVerts, NULL, mVCount / 3);
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
    cityRenderer.areaModel = BakeStaticGeometry(areaVerts, areaColors, aVCount / 3);
    free(areaVerts);
    free(areaColors);

    int maxRoofVerts = map->buildingCount * 12 * 3; 
    float *roofVerts = (float*)malloc(maxRoofVerts * 3 * sizeof(float));
    float *roofColors = (float*)malloc(maxRoofVerts * 4 * sizeof(float)); 
    int rfVCount = 0;

    for (int i = 0; i < map->buildingCount; i++) {
        Building *b = &map->buildings[i];
        if (b->pointCount < 3) continue;
        float y = b->height + 0.1f; 
        Vector2 center = GetBuildingCenter(b->footprint, b->pointCount);
        float rr = 80.0f/255.0f; float rg = 80.0f/255.0f; float rb = 90.0f/255.0f; 

        for (int k = 0; k < b->pointCount; k++) {
            Vector2 p1 = b->footprint[k];
            Vector2 p2 = b->footprint[(k + 1) % b->pointCount];
            roofVerts[rfVCount*3+0] = center.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = center.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
            roofVerts[rfVCount*3+0] = p1.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = p1.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
            roofVerts[rfVCount*3+0] = p2.x; roofVerts[rfVCount*3+1] = y; roofVerts[rfVCount*3+2] = p2.y;
            roofColors[rfVCount*4+0] = rr; roofColors[rfVCount*4+1] = rg; roofColors[rfVCount*4+2] = rb; roofColors[rfVCount*4+3] = 1.0f;
            rfVCount++;
        }
    }
    cityRenderer.roofModel = BakeStaticGeometry(roofVerts, roofColors, rfVCount / 3);
    free(roofVerts);
    free(roofColors);

    cityRenderer.mapBaked = true;
    printf("Baking Done. V-Roads:%d, V-Marks:%d, V-Areas:%d, V-Roofs:%d\n", rVCount/3, mVCount/3, aVCount/3, rfVCount/3);
}

void LoadCityAssets() {
    if (cityRenderer.loaded) return;
    
    Shader instancingShader = LoadShaderFromMemory(INSTANCING_VSH, INSTANCING_FSH);
    instancingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(instancingShader, "instanceTransform");

    Image whiteImg = GenImageColor(1, 1, WHITE);
    cityRenderer.whiteTex = LoadTextureFromImage(whiteImg);
    UnloadImage(whiteImg);

    #define LOAD_ASSET(enumIdx, filename) \
        { \
            char fullPath[256]; \
            sprintf(fullPath, "resources/Buildings/%s", filename); \
            cityRenderer.models[enumIdx] = LoadModel(fullPath); \
            if (cityRenderer.models[enumIdx].meshCount == 0) { \
                cityRenderer.models[enumIdx] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); \
            } else { \
                for(int i = 0; i < cityRenderer.models[enumIdx].materialCount; i++) { \
                      cityRenderer.models[enumIdx].materials[i].shader = instancingShader; \
                      if (cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) { \
                        Texture2D *tex = &cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture; \
                        GenTextureMipmaps(tex); \
                        SetTextureFilter(*tex, TEXTURE_FILTER_TRILINEAR); \
                      } \
                } \
            } \
        }

    // Load Standard Assets
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

    // Procedural Assets
    Model cubeModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Material propMat = LoadMaterialDefault();
    propMat.shader = instancingShader;
    propMat.maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex;
    cubeModel.materials[0] = propMat;

    cityRenderer.models[ASSET_WALL] = cubeModel;
    cityRenderer.models[ASSET_CORNER] = cubeModel;
    cityRenderer.models[ASSET_SIDEWALK] = cubeModel;
    cityRenderer.models[ASSET_PROP_TREE] = cubeModel;
    cityRenderer.models[ASSET_PROP_BENCH] = cubeModel;
    cityRenderer.models[ASSET_PROP_HYDRANT] = cubeModel;
    cityRenderer.models[ASSET_PROP_LIGHT] = cubeModel;
    cityRenderer.models[ASSET_PROP_PARK_TREE] = cubeModel;

    for (int i = 0; i < 5; i++) {
        for (int m = 0; m < ASSET_COUNT; m++) InitRenderBatch(&cityRenderer.batches[i][m], cityPalette[i]);
    }
    for (int m = 0; m < ASSET_COUNT; m++) InitRenderBatch(&cityRenderer.batches[5][m], WHITE);
    
    // Prop Tints
    cityRenderer.batches[5][ASSET_PROP_TREE].tint = (Color){30, 100, 30, 255};      
    cityRenderer.batches[5][ASSET_PROP_BENCH].tint = (Color){100, 70, 40, 255};     
    cityRenderer.batches[5][ASSET_PROP_HYDRANT].tint = (Color){200, 40, 40, 255};   
    cityRenderer.batches[5][ASSET_PROP_LIGHT].tint = (Color){80, 80, 90, 255};      
    cityRenderer.batches[5][ASSET_SIDEWALK].tint = (Color){180, 180, 180, 255};     
    cityRenderer.batches[5][ASSET_PROP_PARK_TREE].tint = (Color){10, 90, 20, 255};  

    cityRenderer.loaded = true;
}

BuildingStyle GetBuildingStyle(Vector2 pos) {
    BuildingStyle style = {0};
    float distToCenter = Vector2Length(pos);
    bool isCenter = (distToCenter < REGION_CENTER_RADIUS);
    int roll = GetRandomValue(0, 100);
    if (isCenter && roll < 60) {
        style.isSkyscraper = true; style.window = ASSET_WIN_TALL; style.windowTop = ASSET_WIN_TALL_TOP;
        style.doorFrame = ASSET_FRAME_DOOR1; style.doorInner = ASSET_DOOR_BROWN_GLASS; style.balcony = ASSET_BALCONY;
        style.hasAC = false; style.isWhiteTheme = false;
    } else if (roll < 30) {
        style.isSkyscraper = false; style.isWhiteTheme = false; style.window = ASSET_WIN_DET;
        style.windowTop = ASSET_WIN_DET; style.doorFrame = ASSET_FRAME_TENT; style.doorInner = ASSET_DOOR_BROWN;
        style.balcony = ASSET_BALCONY; style.hasAC = true;
    } else if (roll < 70) {
        style.isSkyscraper = false; style.isWhiteTheme = true; style.window = ASSET_WIN_TWIN_TENT_W;
        style.windowTop = ASSET_WIN_TWIN_TENT_W; style.doorFrame = ASSET_FRAME_WIN_WHITE;
        style.doorInner = ASSET_DOOR_WHITE_WIN; style.balcony = ASSET_BALCONY_WHITE; style.hasAC = true;
    } else {
        style.isSkyscraper = false; style.isWhiteTheme = false; style.window = ASSET_WIN_SIMPLE;
        style.windowTop = ASSET_WIN_SIMPLE; style.doorFrame = ASSET_FRAME_SIMPLE; style.doorInner = ASSET_DOOR_BROWN_WIN;
        style.balcony = ASSET_BALCONY; style.hasAC = true;
    }
    return style;
}

#define ADD_INSTANCE(group, assetType, pos, rotation, scaleVec) \
    if (cityRenderer.batches[group][assetType].count < MAX_INSTANCES) { \
        Matrix matScale = MatrixScale(scaleVec.x, scaleVec.y, scaleVec.z); \
        Matrix matRot = MatrixRotateY(rotation * DEG2RAD); \
        Matrix matTrans = MatrixTranslate(pos.x, pos.y, pos.z); \
        Matrix final = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans); \
        cityRenderer.batches[group][assetType].transforms[cityRenderer.batches[group][assetType].count++] = final; \
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
        Vector2 toCenter = Vector2Subtract(bCenter, wallMid);
        if (Vector2DotProduct(wallNormal, toCenter) > 0) {
            wallNormal = Vector2Negate(wallNormal); 
            modelRotation += 180.0f;                
        }
        Vector2 cornerInset = Vector2Scale(Vector2Normalize(Vector2Subtract(bCenter, p1)), 0.05f);
        Vector3 cornerPos = { p1.x + cornerInset.x, visualHeight/2.0f, p1.y + cornerInset.y };
        ADD_INSTANCE(colorIdx, ASSET_CORNER, cornerPos, -angle, ((Vector3){cornerThick, visualHeight, cornerThick}));
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
                    ADD_INSTANCE(colorIdx, style.doorFrame, pos, modelRotation, frameScale);
                    Vector3 doorScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth * 0.8f };
                    ADD_INSTANCE(colorIdx, style.doorInner, pos, modelRotation, doorScale);
                } else {
                    bool wantsBalcony = false;
                    if (!style.isSkyscraper && f > 0 && f < floors-1) {
                          if ((m % 2 != 0) && GetRandomValue(0, 100) < 40) wantsBalcony = true;
                    }
                    Vector3 winScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                    if (wantsBalcony) {
                        ADD_INSTANCE(colorIdx, style.balcony, pos, modelRotation, winScale);
                    } else {
                        AssetType winType = (f == floors - 1) ? style.windowTop : style.window;
                        ADD_INSTANCE(colorIdx, winType, pos, modelRotation, winScale);
                        if (style.hasAC && f < floors-1 && GetRandomValue(0, 100) < 15) {
                            AssetType acType = (GetRandomValue(0, 1) == 0) ? ASSET_AC_A : ASSET_AC_B;
                            Vector3 acPos = { pos.x, pos.y - 0.4f, pos.z }; 
                            Vector3 acScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                            ADD_INSTANCE(colorIdx, acType, acPos, modelRotation, acScale);
                        }
                    }
                }
                if (!style.isSkyscraper) {
                    if (f > 0) {
                        Vector3 beamPos = { pos.x, (f * floorHeight), pos.z };
                        ADD_INSTANCE(colorIdx, ASSET_WALL, beamPos, modelRotation, floorBeamScale);
                    }
                }
                if (f == floors - 1) {
                    float corniceOffset = 0.15f; 
                    Vector3 cornicePos = { currentPos2D.x + (wallNormal.x * corniceOffset), visualHeight, currentPos2D.y + (wallNormal.y * corniceOffset) };
                    Vector3 corniceScale = { moduleWidth * 1.05f, beamHeight, structuralDepth };
                    ADD_INSTANCE(colorIdx, ASSET_WALL, cornicePos, modelRotation, corniceScale);
                }
            }
            currentPos2D = Vector2Add(currentPos2D, Vector2Scale(dir, moduleWidth));
        }
        if (remainingSpace > 0.1f) {
            float fillerLen = remainingSpace / 2.0f;
            Vector2 f1 = Vector2Add(p1, Vector2Scale(dir, fillerLen/2.0f));
            Vector3 pos1 = {f1.x, visualHeight/2.0f, f1.y};
            ADD_INSTANCE(colorIdx, ASSET_WALL, pos1, -angle, ((Vector3){fillerLen, visualHeight, structuralDepth}));
            Vector2 f2 = Vector2Add(p2, Vector2Scale(dir, -fillerLen/2.0f));
            Vector3 pos2 = {f2.x, visualHeight/2.0f, f2.y};
            ADD_INSTANCE(colorIdx, ASSET_WALL, pos2, -angle, ((Vector3){fillerLen, visualHeight, structuralDepth}));
        }
    }
}

void BakeRoadDetails(GameMap *map) {
    float sidewalkW = 2.5f; 
    float propSpacing = 15.0f; 

    int *nodeDegree = (int*)calloc(map->nodeCount, sizeof(int));
    for(int i=0; i<map->edgeCount; i++) {
        if(map->edges[i].startNode < map->nodeCount) nodeDegree[map->edges[i].startNode]++;
        if(map->edges[i].endNode < map->nodeCount) nodeDegree[map->edges[i].endNode]++;
    }

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

        // --- CUT LOGIC (SMART) ---
        // Cut sidewalks ONLY at intersections (degree > 2)
        float cutFactor = finalRoadW * 0.85f; 
        float startCut = 0.0f;
        float endCut = 0.0f;
        
        if (nodeDegree[e.startNode] > 2) startCut = cutFactor;
        if (nodeDegree[e.endNode] > 2) endCut = cutFactor;
        
        if (startCut + endCut > len * 0.9f) {
            float factor = (len * 0.9f) / (startCut + endCut);
            startCut *= factor;
            endCut *= factor;
        }

        for(int side=-1; side<=1; side+=2) {
            Vector2 sideOffset = Vector2Scale(right, offsetDist * side);
            Vector2 rawStart = Vector2Add(s, sideOffset);
            Vector2 swStart = Vector2Add(rawStart, Vector2Scale(dir, startCut));
            float swLen = len - (startCut + endCut);
            if (swLen < 0.1f) continue;

            Vector2 swMid = Vector2Add(swStart, Vector2Scale(dir, swLen/2.0f));
            Vector3 swPos = { swMid.x, 0.10f, swMid.y }; 
            Vector3 swScale = { swLen, 0.10f, sidewalkW };
            ADD_INSTANCE(5, ASSET_SIDEWALK, swPos, -angle, swScale);

            int propCount = (int)(swLen / propSpacing);
            for(int p=0; p<propCount; p++) {
                float distAlong = (p * propSpacing) + (propSpacing * 0.5f);
                Vector2 propPos2D = Vector2Add(swStart, Vector2Scale(dir, distAlong));
                
                bool blocked = false;
                for(int L=0; L<map->locationCount; L++) {
                    if(map->locations[L].type != LOC_HOUSE && map->locations[L].type != LOC_FUEL) {
                          if(Vector2Distance(propPos2D, map->locations[L].position) < 8.0f) {
                              blocked = true;
                              break;
                          }
                    }
                }
                if (blocked) continue;

                Vector3 propPos = { propPos2D.x, 0.2f, propPos2D.y };
                float rot = (side == 1) ? -angle : -angle + 180.0f; 

                int roll = GetRandomValue(0, 100);
                if (roll < 40) { 
                      Vector3 scale = { 0.8f, 3.5f, 0.8f }; 
                      ADD_INSTANCE(5, ASSET_PROP_TREE, propPos, rot, scale);
                      Vector3 leavesPos = { propPos.x, 3.0f, propPos.z };
                      Vector3 leavesScale = { 2.5f, 2.0f, 2.5f };
                      ADD_INSTANCE(5, ASSET_PROP_TREE, leavesPos, rot, leavesScale);
                }
                else if (roll < 60) {
                      Vector3 scale = { 2.0f, 0.5f, 0.8f };
                      ADD_INSTANCE(5, ASSET_PROP_BENCH, propPos, rot, scale);
                }
                else if (roll < 70) {
                      Vector3 scale = { 0.5f, 0.8f, 0.5f };
                      ADD_INSTANCE(5, ASSET_PROP_HYDRANT, propPos, rot, scale);
                }
                else if (roll < 85) {
                      Vector3 scale = { 0.3f, 5.0f, 0.3f };
                      ADD_INSTANCE(5, ASSET_PROP_LIGHT, propPos, rot, scale);
                }
            }
        }
    }
    free(nodeDegree);
}

// Helper to clear events
void ClearEvents(GameMap *map) {
    for (int i = 0; i < MAX_EVENTS; i++) {
        map->events[i].active = false;
        map->events[i].timer = 0;
    }
}

// Dev function to trigger specific event
void TriggerSpecificEvent(GameMap *map, MapEventType type, Vector3 playerPos, Vector3 playerFwd) {
    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!map->events[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;

    // Spawn 15 units in front of player
    Vector2 spawnPos;
    spawnPos.x = playerPos.x + (playerFwd.x * 15.0f);
    spawnPos.y = playerPos.z + (playerFwd.z * 15.0f);

    map->events[slot].active = true;
    map->events[slot].type = type;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f; 
    map->events[slot].timer = 30.0f; // 30 seconds for dev events

    if (type == EVENT_CRASH) {
        snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    } else if (type == EVENT_ROADWORK) {
        snprintf(map->events[slot].label, 64, "ROAD WORK");
    }
}

void TriggerRandomEvent(GameMap *map, Vector3 playerPos, Vector3 playerFwd) {
    // 1. Find a Random Slot
    int slot = -1;
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!map->events[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return; // Map full of events

    // 2. Find a Random Road Location (Not on player, but in range)
    int attempts = 0;
    bool found = false;
    Vector2 spawnPos = {0};

    while (attempts < 50) {
        int eIdx = GetRandomValue(0, map->edgeCount - 1);
        Edge e = map->edges[eIdx];
        
        Vector2 p1 = map->nodes[e.startNode].position;
        Vector2 p2 = map->nodes[e.endNode].position;
        Vector2 mid = Vector2Scale(Vector2Add(p1, p2), 0.5f); // Middle of road
        
        float dist = Vector2Distance(mid, (Vector2){playerPos.x, playerPos.z});
        
        // Spawn between 100 and 500 units away
        if (dist > 100.0f && dist < 500.0f) {
            spawnPos = mid;
            found = true;
            break;
        }
        attempts++;
    }

    if (!found) return; // Couldn't find a spot

    // 3. Setup Event
    map->events[slot].active = true;
    map->events[slot].position = spawnPos;
    map->events[slot].radius = 8.0f;
    map->events[slot].timer = 120.0f; // Lasts 2 mins

    // 4. Random Type (50/50 Chance)
    if (GetRandomValue(0, 100) < 50) {
        map->events[slot].type = EVENT_CRASH;
        snprintf(map->events[slot].label, 64, "ACCIDENT ALERT");
    } else {
        map->events[slot].type = EVENT_ROADWORK;
        snprintf(map->events[slot].label, 64, "ROAD WORK");
    }
}

// Dev Controls
void UpdateDevControls(GameMap *map, Vector3 playerPos, Vector3 playerFwd) {
    if (IsKeyPressed(KEY_F1)) {
        TriggerSpecificEvent(map, EVENT_CRASH, playerPos, playerFwd);
        TraceLog(LOG_INFO, "DEV: Spawned Crash");
    }
    if (IsKeyPressed(KEY_F2)) {
        TriggerSpecificEvent(map, EVENT_ROADWORK, playerPos, playerFwd);
        TraceLog(LOG_INFO, "DEV: Spawned Roadwork");
    }
    // [MODIFIED] Changed from F3 to F4 to allow F3 for Mechanic Menu debug
    if (IsKeyPressed(KEY_F4)) {
        ClearEvents(map);
        TraceLog(LOG_INFO, "DEV: Cleared Events");
    }
}

// [HELPER] Draws a label centered on a 3D position
static void DrawCenteredLabel(Camera camera, Vector3 position, const char *text, Color color) {
    Vector2 screenPos = GetWorldToScreen(position, camera);
    if (screenPos.x > 0 && screenPos.x < GetScreenWidth() && 
        screenPos.y > 0 && screenPos.y < GetScreenHeight()) {
        
        int fontSize = 20;
        int textW = MeasureText(text, fontSize);
        int padding = 4;
        
        // Draw Black Background Rectangle
        DrawRectangle(screenPos.x - textW/2 - padding, screenPos.y - fontSize/2 - padding, 
                      textW + padding*2, fontSize + padding*2, BLACK);
                      
        // Draw White Text (Ignore color param for max contrast as requested)
        DrawText(text, (int)screenPos.x - textW/2, (int)screenPos.y - fontSize/2, fontSize, WHITE);
    }
}

// --- RENDER ---
void DrawGameMap(GameMap *map, Camera camera) {
    rlDisableBackfaceCulling(); 
    Vector2 pPos2D = { camera.position.x, camera.position.z };

    // Infinite Floor (Regular Gray)
    DrawPlane((Vector3){0, -0.05f, 0}, (Vector2){10000.0f, 10000.0f}, (Color){80, 80, 80, 255});

    // 1. Static Baked Geometry (Fastest)
    if (cityRenderer.mapBaked) {
        DrawModel(cityRenderer.areaModel, (Vector3){0,0,0}, 1.0f, WHITE);
        DrawModel(cityRenderer.roadModel, (Vector3){0,0,0}, 1.0f, WHITE);
        DrawModel(cityRenderer.markingsModel, (Vector3){0,0,0}, 1.0f, WHITE);
        DrawModel(cityRenderer.roofModel, (Vector3){0,0,0}, 1.0f, WHITE); 
    } 

    // 2. Instanced Buildings
    for (int g = 0; g < BATCH_GROUPS; g++) {
        for (int m = 0; m < ASSET_COUNT; m++) {
            RenderBatch *batch = &cityRenderer.batches[g][m];
            if (batch->count > 0) {
#if USE_INSTANCING
                cityRenderer.models[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = batch->tint;
                DrawMeshInstanced(cityRenderer.models[m].meshes[0], cityRenderer.models[m].materials[0], batch->transforms, batch->count);
#else
                cityRenderer.models[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = batch->tint;
                for(int i=0; i<batch->count; i++) {
                    Matrix mat = batch->transforms[i];
                    Vector3 pos = (Vector3){ mat.m12, mat.m13, mat.m14 };
                    if (Vector2Distance(pPos2D, (Vector2){pos.x, pos.z}) > RENDER_DIST_BASE) continue;
                    cityRenderer.models[m].transform = mat;
                    DrawModel(cityRenderer.models[m], (Vector3){0,0,0}, 1.0f, WHITE);
                }
#endif
            }
        }
    }
    
    // 3. Locations
    for (int i = 0; i < map->locationCount; i++) {
        if (Vector2Distance(pPos2D, map->locations[i].position) > RENDER_DIST_BASE) continue;
        Vector3 pos = { map->locations[i].position.x, 1.0f, map->locations[i].position.y };
        Color poiColor = RED;
        
        // [UPDATED] Draw Gas Pumps at LOC_FUEL
        if(map->locations[i].type == LOC_FUEL) {
            poiColor = ORANGE;
            Vector3 pumpPos = { pos.x + 2.0f, 1.0f, pos.z + 2.0f };
            DrawCube(pumpPos, 1.0f, 2.0f, 1.0f, YELLOW);
            DrawCubeWires(pumpPos, 1.0f, 2.0f, 1.0f, BLACK);
            DrawCube((Vector3){pumpPos.x + 0.51f, pumpPos.y + 0.5f, pumpPos.z}, 0.1f, 0.5f, 0.6f, BLACK); 
        }
        // [NEW] Draw Mechanic Stand at LOC_MECHANIC
        else if(map->locations[i].type == LOC_MECHANIC) {
            poiColor = DARKBLUE;
            Vector3 mechPos = { pos.x + 2.0f, 0.5f, pos.z + 2.0f }; // Corrected Y to 0.5f so it sits on ground
            // Stacked cubes prop
            DrawCube(mechPos, 1.5f, 1.0f, 1.5f, BLUE); // Brighter blue
            DrawCubeWires(mechPos, 1.5f, 1.0f, 1.5f, BLACK);
            DrawCube((Vector3){mechPos.x, 1.5f, mechPos.z}, 1.0f, 1.0f, 1.0f, SKYBLUE); // Brighter top
            DrawCubeWires((Vector3){mechPos.x, 1.5f, mechPos.z}, 1.0f, 1.0f, 1.0f, WHITE);
        }
        else if(map->locations[i].type == LOC_FOOD) poiColor = GREEN;
        else if(map->locations[i].type == LOC_MARKET) poiColor = BLUE;
        
        DrawSphere(pos, 1.5f, Fade(poiColor, 0.8f));
        DrawLine3D(pos, (Vector3){pos.x, 4.0f, pos.z}, BLACK);
    }
    
    // Draw Events
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            Vector3 pos = { map->events[i].position.x, 1.5f, map->events[i].position.y };
            DrawCube(pos, 3.0f, 3.0f, 3.0f, COLOR_EVENT_PROP);
            DrawCubeWires(pos, 3.1f, 3.1f, 3.1f, BLACK);
        }
    }
    
    rlEnableBackfaceCulling();
    
    // --- DRAW LABELS (2D Overlay) ---
    EndMode3D(); 
    
    // A. Draw Event Labels
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            // Label on cube center (Y ~ 1.5f)
            Vector3 pos = { map->events[i].position.x, 1.5f, map->events[i].position.y };
            DrawCenteredLabel(camera, pos, map->events[i].label, COLOR_EVENT_TEXT);
        }
    }

    // B. Draw Placeholder Prop Labels (Close range only)
    Vector3 playerPos = camera.position; 
    float renderLabelDistSq = 20.0f * 20.0f; // 20m range

    // Only iterate prop batches (ASSET_PROP_*)
    for (int g = 0; g < BATCH_GROUPS; g++) {
        for (int m = ASSET_PROP_TREE; m <= ASSET_PROP_PARK_TREE; m++) {
            RenderBatch *batch = &cityRenderer.batches[g][m];
            if (batch->count == 0) continue;

            const char* label = "PROP";
            if (m == ASSET_PROP_TREE) label = "TREE";
            else if (m == ASSET_PROP_BENCH) label = "BENCH";
            else if (m == ASSET_PROP_HYDRANT) label = "HYDRANT";
            else if (m == ASSET_PROP_LIGHT) label = "LIGHT";
            else if (m == ASSET_PROP_PARK_TREE) label = "PARK TREE";

            for(int i=0; i<batch->count; i++) {
                Vector3 pos = { batch->transforms[i].m12, batch->transforms[i].m13 + 1.0f, batch->transforms[i].m14 };
                if (Vector3DistanceSqr(pos, playerPos) < renderLabelDistSq) {
                    DrawCenteredLabel(camera, pos, label, BLACK);
                }
            }
        }
    }
    
    // [NEW] Draw Interaction Prompts for Fuel/Mechanic
    for (int i = 0; i < map->locationCount; i++) {
        if (map->locations[i].type == LOC_FUEL || map->locations[i].type == LOC_MECHANIC) {
            Vector3 targetPos = { map->locations[i].position.x + 2.0f, 1.5f, map->locations[i].position.y + 2.0f };
            // [UPDATED] Check radius to match main.c interaction (12.0f -> 144.0f squared)
            if (Vector3DistanceSqr(targetPos, playerPos) < 144.0f) { 
                const char* txt = (map->locations[i].type == LOC_FUEL) ? "Refuel [E]" : "Mechanic [E]";
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

bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    Vector2 p = { x, z };
    for (int i = 0; i < map->buildingCount; i++) {
        Vector2 center = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        if (Vector2Distance(p, center) > 25.0f) continue; 
        if (CheckCollisionPointPoly(p, map->buildings[i].footprint, map->buildings[i].pointCount)) return true;
    }
    for(int i=0; i<MAX_EVENTS; i++) {
        if (map->events[i].active) {
            if (Vector2Distance(p, map->events[i].position) < (map->events[i].radius + radius)) return true;
        }
    }
    return false;
}

void UnloadGameMap(GameMap *map) {
    if (map->nodes) free(map->nodes);
    if (map->edges) free(map->edges);
    if (map->buildingCount > 0) {
        for (int i = 0; i < map->buildingCount; i++) free(map->buildings[i].footprint);
    }
    if (map->areaCount > 0) {
        for (int i = 0; i < map->areaCount; i++) free(map->areas[i].points);
    }
    if (map->buildings) free(map->buildings);
    if (map->locations) free(map->locations);
    if (map->areas) free(map->areas);
    if (map->graph) {
        for(int i=0; i<map->nodeCount; i++) if(map->graph[i].connections) free(map->graph[i].connections);
        free(map->graph);
    }
    if (cityRenderer.loaded) {
        UnloadTexture(cityRenderer.whiteTex);
        for (int i = 0; i < BATCH_GROUPS; i++) {
            for (int m = 0; m < ASSET_COUNT; m++) if(cityRenderer.batches[i][m].transforms) free(cityRenderer.batches[i][m].transforms);
        }
        
        for (int m = 0; m < ASSET_WALL; m++) UnloadModel(cityRenderer.models[m]);
        UnloadModel(cityRenderer.models[ASSET_WALL]);
        
        if(cityRenderer.mapBaked) {
            UnloadModel(cityRenderer.roadModel);
            UnloadModel(cityRenderer.markingsModel);
            UnloadModel(cityRenderer.areaModel);
            UnloadModel(cityRenderer.roofModel); 
        }
        
        cityRenderer.loaded = false;
    }
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
        float d = Vector2DistanceSqr(position, map->nodes[i].position);
        if (d < minDst) { minDst = d; bestNode = i; }
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

GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));
    map.areas = (MapArea *)calloc(MAX_AREAS, sizeof(MapArea));
    map.isBatchLoaded = false;
    
    // [NEW] Init Events
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

    printf("Baking City Geometry...\n");
    for (int i = 0; i < map.buildingCount; i++) {
        BakeBuildingGeometry(&map.buildings[i]);
    }
    BakeRoadDetails(&map); 
    BakeMapElements(&map); 

    int totalInstances = 0;
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int m=0; m<ASSET_COUNT; m++) totalInstances += cityRenderer.batches[g][m].count;
    }
    printf("Baking Complete. Total Instances: %d\n", totalInstances);

    return map;
}
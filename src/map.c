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
const float RENDER_DISTANCE = 2000.0f;
#define MODEL_SCALE 1.8f         // Matched to car
#define MODEL_Z_SQUISH 0.4f      // Squished depth
#define MAX_INSTANCES 50000 
#define BATCH_GROUPS 6 
#define USE_INSTANCING 1         // Must be 1 for performance

// Define Regions
#define REGION_CENTER_RADIUS 600.0f // Buildings within 600m are "City Center"

typedef enum {
    // --- SPECIALS ---
    ASSET_AC_A = 0,
    ASSET_AC_B,
    ASSET_BALCONY,
    ASSET_BALCONY_WHITE,
    
    // --- DOORS (INNER) ---
    ASSET_DOOR_BROWN,
    ASSET_DOOR_BROWN_GLASS,
    ASSET_DOOR_BROWN_WIN,
    ASSET_DOOR_WHITE,
    ASSET_DOOR_WHITE_GLASS,
    ASSET_DOOR_WHITE_WIN,

    // --- DOOR FRAMES (OUTER) ---
    ASSET_FRAME_DOOR1,
    ASSET_FRAME_SIMPLE,
    ASSET_FRAME_TENT,
    ASSET_FRAME_WIN,
    ASSET_FRAME_WIN_WHITE,

    // --- WINDOWS (REGULAR) ---
    ASSET_WIN_SIMPLE,
    ASSET_WIN_SIMPLE_W,
    ASSET_WIN_DET,
    ASSET_WIN_DET_W,
    ASSET_WIN_TWIN_TENT,
    ASSET_WIN_TWIN_TENT_W,

    // --- WINDOWS (SKYSCRAPER) ---
    ASSET_WIN_TALL,
    ASSET_WIN_TALL_TOP,

    // --- PROCEDURAL ---
    ASSET_WALL,
    ASSET_CORNER,
    
    ASSET_COUNT
} AssetType;

// Style Definition used during generation
typedef struct {
    AssetType window;
    AssetType windowTop; // Usually same as window, unless skyscraper
    AssetType doorFrame;
    AssetType doorInner;
    AssetType balcony;
    bool hasAC;
    bool isSkyscraper;   // If true, no beams + continuous glass
    bool isWhiteTheme;
} BuildingStyle;

typedef struct {
    Matrix *transforms; 
    int count;          
    Color tint;         
} RenderBatch;

#define BATCH_GROUPS 6 

typedef struct {
    Model models[ASSET_COUNT];       
    RenderBatch batches[BATCH_GROUPS][ASSET_COUNT]; 
    bool loaded;
    Texture2D whiteTex;
} CityRenderSystem;

static CityRenderSystem cityRenderer = {0};

Color cityPalette[] = {
    {152, 251, 152, 255}, 
    {255, 182, 193, 255}, 
    {255, 105, 97, 255},  
    {255, 200, 150, 255}, 
    {200, 200, 200, 255}  
};

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
    for(int i = 0; i < count; i++) center = Vector2Add(center, footprint[i]);
    center.x /= count; center.y /= count;
    return center;
}

// --- INIT ---
void InitRenderBatch(RenderBatch *batch, Color color) {
    batch->transforms = (Matrix *)malloc(MAX_INSTANCES * sizeof(Matrix));
    batch->count = 0;
    batch->tint = color;
}

// --- EMBEDDED SHADER FOR INSTANCING ---
static const char* INSTANCING_VSH = 
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec2 vertexTexCoord;\n"
    "in vec3 vertexNormal;\n"
    "in mat4 instanceTransform;\n"
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "out vec3 fragNormal;\n" // <--- Pass this to Fragment Shader
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    fragTexCoord = vertexTexCoord;\n"
    "    fragColor = vec4(1.0);\n"
    
    // We rotate the normal using the instance matrix (casting to mat3 removes translation)
    "    fragNormal = mat3(instanceTransform) * vertexNormal;\n"
    
    "    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* INSTANCING_FSH = 
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "in vec3 fragNormal;\n" // <--- We need the normal from Vertex Shader
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    
    // Hardcoded "Sun" direction (coming from top-right)
    "const vec3 lightDir = normalize(vec3(0.5, 0.8, 0.3));\n" 

    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    // Calculate simplified diffuse lighting (0.4 ambient + 0.6 directional)\n"
    "    float lightDot = max(dot(normalize(fragNormal), lightDir), 0.0);\n"
    "    float light = 0.4 + (0.6 * lightDot);\n" 
    "    \n"
    "    finalColor = texelColor * colDiffuse * fragColor * vec4(light, light, light, 1.0);\n"
    "}\n";

void DrawBuildingRoof(Building *b) {
    if (b->pointCount < 3) return;
    
    Vector2 center = GetBuildingCenter(b->footprint, b->pointCount);
    
    // [FIX 3] Lift roof slightly to avoid Z-fighting with wall tops
    float y = b->height + 0.1f; 

    rlBegin(RL_TRIANGLES);
        rlColor4ub(80, 80, 90, 255); 

        for (int i = 0; i < b->pointCount; i++) {
            Vector2 p1 = b->footprint[i];
            Vector2 p2 = b->footprint[(i + 1) % b->pointCount];

            rlVertex3f(center.x, y, center.y);
            rlVertex3f(p1.x, y, p1.y);
            rlVertex3f(p2.x, y, p2.y);
        }
    rlEnd();
}

void LoadCityAssets() {
    if (cityRenderer.loaded) return;
    printf("--- INITIALIZING CITY RENDERER (Instancing: %s) ---\n", USE_INSTANCING ? "ON" : "OFF");

    Shader instancingShader = LoadShaderFromMemory(INSTANCING_VSH, INSTANCING_FSH);
    instancingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(instancingShader, "instanceTransform");

    Image whiteImg = GenImageColor(1, 1, WHITE);
    cityRenderer.whiteTex = LoadTextureFromImage(whiteImg);
    UnloadImage(whiteImg);

    // UPDATED MACRO: Applies shader and texture settings to ALL materials in a model
    #define LOAD_ASSET(enumIdx, filename) \
        { \
            char fullPath[256]; \
            sprintf(fullPath, "resources/Buildings/%s", filename); \
            cityRenderer.models[enumIdx] = LoadModel(fullPath); \
            if (cityRenderer.models[enumIdx].meshCount == 0) { \
                printf("[ERROR] MODEL MISSING: %s (Check filename/path!)\n", fullPath); \
                cityRenderer.models[enumIdx] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); \
            } else { \
                /* Iterate through all materials to apply settings */ \
                for(int i = 0; i < cityRenderer.models[enumIdx].materialCount; i++) { \
                     /* Apply Instancing Shader */ \
                     cityRenderer.models[enumIdx].materials[i].shader = instancingShader; \
                     \
                     /* Apply Texture Filtering (if a texture exists) */ \
                     if (cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) { \
                        Texture2D *tex = &cityRenderer.models[enumIdx].materials[i].maps[MATERIAL_MAP_DIFFUSE].texture; \
                        GenTextureMipmaps(tex); \
                        SetTextureFilter(*tex, TEXTURE_FILTER_TRILINEAR); \
                     } \
                } \
            } \
        }

    // --- LOAD ALL ASSETS (List unchanged) ---
    LOAD_ASSET(ASSET_AC_A, "detail-ac-a.obj");
    LOAD_ASSET(ASSET_AC_B, "detail-ac-b.obj");
    LOAD_ASSET(ASSET_BALCONY, "balcony.obj");
    LOAD_ASSET(ASSET_BALCONY_WHITE, "balcony_white.obj");

    // Doors (Inner)
    LOAD_ASSET(ASSET_DOOR_BROWN, "door-brown.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_GLASS, "door-brown-glass.obj");
    LOAD_ASSET(ASSET_DOOR_BROWN_WIN, "door-brown-window.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE, "door-white.obj"); 
    LOAD_ASSET(ASSET_DOOR_WHITE_GLASS, "door-white-glass.obj");
    LOAD_ASSET(ASSET_DOOR_WHITE_WIN, "door-white-window.obj");

    // Door Frames
    LOAD_ASSET(ASSET_FRAME_DOOR1, "door1.obj");
    LOAD_ASSET(ASSET_FRAME_SIMPLE, "simple_door.obj");
    LOAD_ASSET(ASSET_FRAME_TENT, "doorframe_glass_tent.obj");
    LOAD_ASSET(ASSET_FRAME_WIN, "window_door.obj");
    LOAD_ASSET(ASSET_FRAME_WIN_WHITE, "window_door_white.obj");

    // Windows
    LOAD_ASSET(ASSET_WIN_SIMPLE, "Windows_simple.obj");
    LOAD_ASSET(ASSET_WIN_SIMPLE_W, "Windows_simple_white.obj");
    LOAD_ASSET(ASSET_WIN_DET, "Windows_detailed.obj");
    LOAD_ASSET(ASSET_WIN_DET_W, "Windows_detailed_white.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT, "Twin_window_tents.obj");
    LOAD_ASSET(ASSET_WIN_TWIN_TENT_W, "Twin_window_tents_white.obj");

    // Skyscraper
    LOAD_ASSET(ASSET_WIN_TALL, "windows_tall.obj");
    LOAD_ASSET(ASSET_WIN_TALL_TOP, "windows_tall_top.obj");

    // Procedural Assets
    cityRenderer.models[ASSET_WALL] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    cityRenderer.models[ASSET_CORNER] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    
    Material procMat = LoadMaterialDefault();
    procMat.shader = instancingShader; 
    procMat.maps[MATERIAL_MAP_DIFFUSE].texture = cityRenderer.whiteTex; 
    procMat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    
    cityRenderer.models[ASSET_WALL].materials[0] = procMat;
    cityRenderer.models[ASSET_CORNER].materials[0] = procMat;

    // Buffers
    for (int i = 0; i < 5; i++) {
        for (int m = 0; m < ASSET_COUNT; m++) InitRenderBatch(&cityRenderer.batches[i][m], cityPalette[i]);
    }
    for (int m = 0; m < ASSET_COUNT; m++) InitRenderBatch(&cityRenderer.batches[5][m], WHITE);

    cityRenderer.loaded = true;
}

BuildingStyle GetBuildingStyle(Vector2 pos) {
    BuildingStyle style = {0};
    
    // Check Region
    float distToCenter = Vector2Length(pos);
    bool isCenter = (distToCenter < REGION_CENTER_RADIUS);
    
    int roll = GetRandomValue(0, 100);

    if (isCenter && roll < 60) {
        // --- STYLE: MODERN SKYSCRAPER ---
        // High chance in city center
        style.isSkyscraper = true;
        style.window = ASSET_WIN_TALL;
        style.windowTop = ASSET_WIN_TALL_TOP;
        style.doorFrame = ASSET_FRAME_DOOR1;
        style.doorInner = ASSET_DOOR_BROWN_GLASS;
        style.balcony = ASSET_BALCONY; // Less likely to have balconies, but possible
        style.hasAC = false; // Skyscrapers usually have central air (hidden)
        style.isWhiteTheme = false;
    } 
    else if (roll < 30) {
        // --- STYLE: VINTAGE / DETAILED ---
        style.isSkyscraper = false;
        style.isWhiteTheme = false;
        style.window = ASSET_WIN_DET;
        style.windowTop = ASSET_WIN_DET;
        style.doorFrame = ASSET_FRAME_TENT;
        style.doorInner = ASSET_DOOR_BROWN;
        style.balcony = ASSET_BALCONY;
        style.hasAC = true;
    }
    else if (roll < 70) {
        // --- STYLE: WHITE MODERN RESIDENTIAL ---
        style.isSkyscraper = false;
        style.isWhiteTheme = true;
        style.window = ASSET_WIN_TWIN_TENT_W;
        style.windowTop = ASSET_WIN_TWIN_TENT_W;
        style.doorFrame = ASSET_FRAME_WIN_WHITE;
        style.doorInner = ASSET_DOOR_WHITE_WIN;
        style.balcony = ASSET_BALCONY_WHITE;
        style.hasAC = true;
    }
    else {
        // --- STYLE: BASIC APARTMENT ---
        style.isSkyscraper = false;
        style.isWhiteTheme = false;
        style.window = ASSET_WIN_SIMPLE;
        style.windowTop = ASSET_WIN_SIMPLE;
        style.doorFrame = ASSET_FRAME_SIMPLE;
        style.doorInner = ASSET_DOOR_BROWN_WIN;
        style.balcony = ASSET_BALCONY;
        style.hasAC = true;
    }
    
    return style;
}

// --- BAKING ---
void BakeBuildingGeometry(Building *b) {
    float floorHeight = 3.0f * (MODEL_SCALE / 4.0f); 
    
    // 1. Determine Style
    Vector2 bCenter = GetBuildingCenter(b->footprint, b->pointCount);
    BuildingStyle style = GetBuildingStyle(bCenter);

    // 2. Skyscraper Compression
    // Squeeze floors closer for skyscrapers (no horizontal beams)
    if (style.isSkyscraper) {
        floorHeight *= 0.85f; 
    }

    // 3. Calculate Floor Count
    int rawFloors = (int)(b->height / floorHeight);
    
    if (style.isSkyscraper) {
        if (rawFloors < 6) rawFloors = 6; 
    } else {
        if (rawFloors < 2) rawFloors = 2;
        if (rawFloors > 5) rawFloors = 5;
    }
    
    int floors = rawFloors;
    float visualHeight = floors * floorHeight;
    b->height = visualHeight; 
    
    int colorIdx = (abs((int)b->footprint[0].x) + abs((int)b->footprint[0].y)) % 5;
    if (style.isWhiteTheme) colorIdx = 5; 

    // Dimensions
    float structuralDepth = MODEL_SCALE * MODEL_Z_SQUISH; 
    float cornerThick = structuralDepth * 0.85f; 

    #define ADD_INSTANCE(group, assetType, pos, rotation, scaleVec) \
        if (cityRenderer.batches[group][assetType].count < MAX_INSTANCES) { \
            Matrix matScale = MatrixScale(scaleVec.x, scaleVec.y, scaleVec.z); \
            Matrix matRot = MatrixRotateY(rotation * DEG2RAD); \
            Matrix matTrans = MatrixTranslate(pos.x, pos.y, pos.z); \
            Matrix final = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans); \
            cityRenderer.batches[group][assetType].transforms[cityRenderer.batches[group][assetType].count++] = final; \
        }

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

        // --- CORNER ---
        Vector2 cornerInset = Vector2Scale(Vector2Normalize(Vector2Subtract(bCenter, p1)), 0.05f);
        Vector3 cornerPos = { p1.x + cornerInset.x, visualHeight/2.0f, p1.y + cornerInset.y };
        ADD_INSTANCE(colorIdx, ASSET_CORNER, cornerPos, -angle, ((Vector3){cornerThick, visualHeight, cornerThick}));

        // --- MODULES ---
        float moduleWidth = 2.0f * (MODEL_SCALE / 4.0f); 
        int modulesCount = (int)(dist / moduleWidth);
        float remainingSpace = dist - (modulesCount * moduleWidth);
        float startOffset = (remainingSpace / 2.0f) + (moduleWidth / 2.0f);
        Vector2 currentPos2D = Vector2Add(p1, Vector2Scale(dir, startOffset)); 
        float outwardOffset = 0.35f; 

        // Beams
        float beamHeight = 0.3f;
        float beamDepth = structuralDepth * 0.25f; 
        Vector3 floorBeamScale = { moduleWidth * 1.05f, beamHeight, beamDepth };

        for (int m = 0; m < modulesCount; m++) {
            for (int f = 0; f < floors; f++) {
                float yPos = (f * floorHeight) + 0.1f;
                Vector3 pos = { currentPos2D.x + (wallNormal.x * outwardOffset), yPos, currentPos2D.y + (wallNormal.y * outwardOffset) };

                // Define Bools for Logic Clarity
                bool isDoor = (f == 0 && m == modulesCount / 2);
                
                // --- LOGIC TREE: DOOR vs EVERYTHING ELSE ---
                
                if (isDoor) {
                    // 1. RENDER DOOR (Exclusive)
                    // Frame
                    Vector3 frameScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                    ADD_INSTANCE(colorIdx, style.doorFrame, pos, modelRotation, frameScale);
                    
                    // Inner Door
                    Vector3 doorScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth * 0.8f };
                    ADD_INSTANCE(colorIdx, style.doorInner, pos, modelRotation, doorScale);
                } 
                else {
                    // 2. RENDER WINDOW OR BALCONY
                    
                    // Determine if Balcony
                    bool wantsBalcony = false;
                    
                    // Balcony Rules:
                    // 1. Not a skyscraper
                    // 2. Not ground, not roof
                    // 3. Random chance (40%)
                    // 4. SPACING RULE: (m % 2 != 0) -> Only allow on odd modules to prevent horizontal clipping
                    if (!style.isSkyscraper && f > 0 && f < floors-1) {
                         if ((m % 2 != 0) && GetRandomValue(0, 100) < 40) {
                             wantsBalcony = true;
                         }
                    }

                    Vector3 winScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };

                    if (wantsBalcony) {
                        // --- BALCONY ---
                        ADD_INSTANCE(colorIdx, style.balcony, pos, modelRotation, winScale);
                    } 
                    else {
                        // --- WINDOW ---
                        AssetType winType = (f == floors - 1) ? style.windowTop : style.window;
                        ADD_INSTANCE(colorIdx, winType, pos, modelRotation, winScale);
                        
                        // --- EXTRAS: AC UNITS (Only attached to windows) ---
                        if (style.hasAC && f < floors-1 && GetRandomValue(0, 100) < 15) {
                            AssetType acType = (GetRandomValue(0, 1) == 0) ? ASSET_AC_A : ASSET_AC_B;
                            Vector3 acPos = { pos.x, pos.y - 0.4f, pos.z }; 
                            Vector3 acScale = { MODEL_SCALE, MODEL_SCALE, structuralDepth };
                            ADD_INSTANCE(colorIdx, acType, acPos, modelRotation, acScale);
                        }
                    }
                }

                // --- STRUCTURAL BEAMS ---
                // No beams on skyscrapers (glass curtain look)
                if (!style.isSkyscraper) {
                    if (f > 0) {
                        Vector3 beamPos = { pos.x, (f * floorHeight), pos.z };
                        ADD_INSTANCE(colorIdx, ASSET_WALL, beamPos, modelRotation, floorBeamScale);
                    }
                }

                // ROOF CORNICE
                if (f == floors - 1) {
                    float corniceOffset = 0.15f; 
                    Vector3 cornicePos = { currentPos2D.x + (wallNormal.x * corniceOffset), visualHeight, currentPos2D.y + (wallNormal.y * corniceOffset) };
                    Vector3 corniceScale = { moduleWidth * 1.05f, beamHeight, structuralDepth };
                    ADD_INSTANCE(colorIdx, ASSET_WALL, cornicePos, modelRotation, corniceScale);
                }
            }
            currentPos2D = Vector2Add(currentPos2D, Vector2Scale(dir, moduleWidth));
        }

        // --- FILLERS ---
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

// --- RENDER ---
void DrawGameMap(GameMap *map, Vector3 playerPos) {
    rlDisableBackfaceCulling(); 
    Vector2 pPos2D = { playerPos.x, playerPos.z };

    // 1. Areas
    for(int i=0; i<map->areaCount; i++) {
        if (map->areas[i].pointCount > 0 && Vector2Distance(pPos2D, map->areas[i].points[0]) > RENDER_DISTANCE) continue;
        Vector3 center = {0, 0.01f, 0};
        for(int j=0; j<map->areas[i].pointCount; j++) { center.x += map->areas[i].points[j].x; center.z += map->areas[i].points[j].y; }
        center.x /= map->areas[i].pointCount; center.z /= map->areas[i].pointCount;
        for(int j=0; j<map->areas[i].pointCount; j++) {
            Vector2 p1 = map->areas[i].points[j]; Vector2 p2 = map->areas[i].points[(j+1)%map->areas[i].pointCount];
            DrawTriangle3D(center, (Vector3){p2.x, 0.01f, p2.y}, (Vector3){p1.x, 0.01f, p1.y}, map->areas[i].color);
        }
    }

    // 2. Roads
    for (int i = 0; i < map->edgeCount; i++) {
        Edge e = map->edges[i];
        if(e.startNode >= map->nodeCount || e.endNode >= map->nodeCount) continue;
        
        Vector2 s = map->nodes[e.startNode].position; 
        Vector2 en = map->nodes[e.endNode].position;
        
        // Distance Culling
        if (Vector2Distance(pPos2D, s) > RENDER_DISTANCE) continue; 
        
        Vector3 start = {s.x, 0.02f, s.y}; 
        Vector3 end = {en.x, 0.02f, en.y};
        Vector3 diff = Vector3Subtract(end, start);
        
        if (Vector3Length(diff) < 0.001f) continue;

        // --- SIMPLIFIED WIDTH LOGIC ---
        // Just double the width from the map file as requested
        float finalWidth = (e.width * MAP_SCALE) * 2.0f;

        Vector3 right = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, diff));
        Vector3 halfWidthVec = Vector3Scale(right, finalWidth * 0.5f);
        
        // --- DRAW ASPHALT ---
        Vector3 v1 = Vector3Subtract(start, halfWidthVec); 
        Vector3 v2 = Vector3Add(start, halfWidthVec);      
        Vector3 v3 = Vector3Add(end, halfWidthVec);        
        Vector3 v4 = Vector3Subtract(end, halfWidthVec);   
        
        DrawTriangle3D(v1, v3, v2, DARKGRAY); 
        DrawTriangle3D(v1, v4, v3, DARKGRAY); 

        // --- DRAW LANE MARKINGS ---
        // Only for two-way roads (separate directions of traffic)
        if (!e.oneway) {
            // Yellow Center Line
            float lineWidth = finalWidth * 0.05f; // Line is 5% of road width
            Vector3 lineOffset = Vector3Scale(right, lineWidth * 0.5f);
            
            // Lift markings slightly (0.035f) to sit ON TOP of asphalt (0.02f)
            Vector3 mStart = { start.x, 0.035f, start.z };
            Vector3 mEnd = { end.x, 0.035f, end.z };

            Vector3 l1 = Vector3Subtract(mStart, lineOffset);
            Vector3 l2 = Vector3Add(mStart, lineOffset);
            Vector3 l3 = Vector3Add(mEnd, lineOffset);
            Vector3 l4 = Vector3Subtract(mEnd, lineOffset);

            DrawTriangle3D(l1, l3, l2, GOLD); 
            DrawTriangle3D(l1, l4, l3, GOLD);
        }
    }

    for (int i = 0; i < map->buildingCount; i++) {
        // Simple distance cull
        Vector2 bPos = map->buildings[i].footprint[0];
        if (Vector2Distance(pPos2D, bPos) > RENDER_DISTANCE) continue;

        DrawBuildingRoof(&map->buildings[i]);
    }

    // 3. CITY
    for (int g = 0; g < BATCH_GROUPS; g++) {
        for (int m = 0; m < ASSET_COUNT; m++) {
            RenderBatch *batch = &cityRenderer.batches[g][m];
            if (batch->count > 0) {
#if DRAW_DEBUG_WIRES
                // DEBUG: Wireframes
                for(int i=0; i<batch->count; i++) {
                    Matrix mat = batch->transforms[i];
                    Vector3 pos = (Vector3){ mat.m12, mat.m13, mat.m14 };
                    DrawCubeWires(pos, MODEL_SCALE, MODEL_SCALE, MODEL_SCALE, RED);
                }
#elif USE_INSTANCING
                // FAST MODE: Instancing
                cityRenderer.models[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = batch->tint;
                DrawMeshInstanced(cityRenderer.models[m].meshes[0], cityRenderer.models[m].materials[0], batch->transforms, batch->count);
#else
                // SAFE MODE: Manual Loop
                // Since wireframes work, this MUST work.
                cityRenderer.models[m].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = batch->tint;
                for(int i=0; i<batch->count; i++) {
                    // Extract position from matrix to check render distance
                    Matrix mat = batch->transforms[i];
                    Vector3 pos = (Vector3){ mat.m12, mat.m13, mat.m14 };
                    if (Vector2Distance(pPos2D, (Vector2){pos.x, pos.z}) > RENDER_DISTANCE) continue;

                    // Set transform and draw
                    cityRenderer.models[m].transform = mat;
                    DrawModel(cityRenderer.models[m], (Vector3){0,0,0}, 1.0f, WHITE);
                }
#endif
            }
        }
    }
    
    // 4. Locations
    for (int i = 0; i < map->locationCount; i++) {
        if (Vector2Distance(pPos2D, map->locations[i].position) > RENDER_DISTANCE) continue;
        Vector3 pos = { map->locations[i].position.x, 1.0f, map->locations[i].position.y };
        Color poiColor = RED;
        if(map->locations[i].type == LOC_FUEL) poiColor = ORANGE;
        else if(map->locations[i].type == LOC_FOOD) poiColor = GREEN;
        else if(map->locations[i].type == LOC_MARKET) poiColor = BLUE;
        DrawSphere(pos, 1.5f, Fade(poiColor, 0.8f));
        DrawLine3D(pos, (Vector3){pos.x, 4.0f, pos.z}, BLACK);
    }
    rlEnableBackfaceCulling();
}

// --- UTILS ---
void UpdateMapEffects(GameMap *map, Vector3 playerPos) {
    for (int i = 0; i < map->locationCount; i++) {
        Vector3 locPos = { map->locations[i].position.x, 0.5f, map->locations[i].position.y };
        if (Vector3Distance(playerPos, locPos) < 50.0f) {
            float pulse = (sinf(GetTime() * 5.0f) + 1.0f) * 0.5f; 
            DrawCircle3D(locPos, 4.0f + pulse, (Vector3){1,0,0}, 90.0f, Fade(RED, 0.3f));
            DrawCubeWires(locPos, 2.0f, 10.0f, 2.0f, Fade(GOLD, 0.5f));
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

bool CheckMapCollision(GameMap *map, float x, float z, float radius) {
    Vector2 p = { x, z };
    for (int i = 0; i < map->buildingCount; i++) {
        Vector2 center = GetBuildingCenter(map->buildings[i].footprint, map->buildings[i].pointCount);
        if (Vector2Distance(p, center) > 25.0f) continue; 
        if (CheckCollisionPointPoly(p, map->buildings[i].footprint, map->buildings[i].pointCount)) return true;
    }
    return false;
}

void UnloadGameMap(GameMap *map) {
    if (map->nodes) free(map->nodes);
    if (map->edges) free(map->edges);
    for (int i = 0; i < map->buildingCount; i++) free(map->buildings[i].footprint);
    for (int i = 0; i < map->areaCount; i++) free(map->areas[i].points);
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
        for (int m = 0; m < ASSET_COUNT; m++) UnloadModel(cityRenderer.models[m]);
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

// --- PARSING ---
GameMap LoadGameMap(const char *fileName) {
    GameMap map = {0};
    map.nodes = (Node *)calloc(MAX_NODES, sizeof(Node));
    map.edges = (Edge *)calloc(MAX_EDGES, sizeof(Edge));
    map.buildings = (Building *)calloc(MAX_BUILDINGS, sizeof(Building));
    map.locations = (MapLocation *)calloc(MAX_LOCATIONS, sizeof(MapLocation));
    map.areas = (MapArea *)calloc(MAX_AREAS, sizeof(MapArea));
    map.isBatchLoaded = false;

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

    // --- BAKE ---
    printf("Baking City Geometry for %d buildings...\n", map.buildingCount);
    for (int i = 0; i < map.buildingCount; i++) {
        BakeBuildingGeometry(&map.buildings[i]);
    }
    
    int totalInstances = 0;
    for(int g=0; g<BATCH_GROUPS; g++) {
        for(int m=0; m<ASSET_COUNT; m++) totalInstances += cityRenderer.batches[g][m].count;
    }
    printf("Baking Complete. Total Instances: %d\n", totalInstances);
    
    // DEBUG MATRIX PRINT
    if (cityRenderer.batches[0][ASSET_WALL].count > 0) {
        Matrix m = cityRenderer.batches[0][ASSET_WALL].transforms[0];
        printf("DEBUG: First Wall Matrix -> T: %.2f, %.2f, %.2f\n", m.m12, m.m13, m.m14);
    }

    return map;
}
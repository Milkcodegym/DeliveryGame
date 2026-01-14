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

#include "start_menu.h" 
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- CONFIG ---
#define MAX_BG_BUILDINGS 1500 
#define MAX_BG_WINDOWS 5000 

// --- COLORS ---
#define COLOR_SKY       (Color){ 10, 10, 15, 255 }   
#define COLOR_FOG       (Color){ 10, 10, 15, 255 }   
#define COLOR_BUILDING  (Color){ 25, 25, 30, 255 }   
#define COLOR_WINDOW    (Color){ 255, 180, 50, 255 } 

// --- SHARED STATE ---
static float sharedProgress = 0.0f;
static int sharedStage = 0;

// --- 1. LOCAL ASSET MANAGEMENT ---
typedef struct {
    Texture2D atlas;
    
    Model light;
    Model tree;
    Model trash;
    
    // CPU Arrays
    Vector3* bPos;
    Vector3* bSize;
    Vector3* wPos;
    Vector3* wSize;
    
    bool loaded;
} MenuAssets;

static MenuAssets menuAssets = {0};

float RandF(float min, float max) {
    return min + (float)GetRandomValue(0, 10000)/10000.0f * (max - min);
}

// --- NEW CONFIGURATION ---
#define SAVE_FILE "save_data.dat"
#define CONFIG_FILE "map_config.dat"

// 1 = Small (Performance), 2 = Big (Gameplay)
static int selectedMapOption = 1; 

// Helper to save the choice so main.c can read it later
void SaveMapChoice(int choice) {
    FILE *f = fopen(CONFIG_FILE, "wb");
    if (f) {
        fwrite(&choice, sizeof(int), 1, f);
        fclose(f);
    }
}

const unsigned char MENU_KEY = 0xAA; 

void MenuObfuscate(unsigned char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= MENU_KEY;
    }
}

// 0 = Continue, 1 = New Game
static int selectedMainMenuOption = 0; 

void DeleteSaveData() {
    if (FileExists(SAVE_FILE)) remove(SAVE_FILE);
    if (FileExists(CONFIG_FILE)) remove(CONFIG_FILE);
    // Reset selection vars
    selectedMainMenuOption = 1; // Default to New Game since Continue is invalid now
}

// --- UI HELPER ---
void DrawSelectionBox(int x, int y, int w, int h, const char* title, const char* desc, bool selected) {
    Color bg = selected ? (Color){40, 60, 100, 200} : (Color){20, 20, 30, 200};
    Color border = selected ? YELLOW : GRAY;
    
    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLinesEx((Rectangle){x,y,w,h}, selected ? 3 : 1, border);
    
    DrawText(title, x + 20, y + 20, 30, selected ? WHITE : LIGHTGRAY);
    DrawText(desc, x + 20, y + 60, 20, GRAY);
    
    if (selected) {
        DrawText("- SELECTED -", x + w - 160, y + h - 30, 20, YELLOW);
    }
}

// [OPTIMIZATION] High-Performance Cube Batcher
// This writes raw vertex data to the buffer, bypassing Raylib's internal overhead
void DrawCubeBatched(Vector3 position, Vector3 size, Color color) {
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float width = size.x / 2.0f;
    float height = size.y / 2.0f;
    float length = size.z / 2.0f;

    rlColor4ub(color.r, color.g, color.b, color.a);

    // Front Face
    rlVertex3f(x - width, y - height, z + length);
    rlVertex3f(x + width, y - height, z + length);
    rlVertex3f(x + width, y + height, z + length);
    rlVertex3f(x - width, y + height, z + length);

    // Back Face
    rlVertex3f(x - width, y + height, z - length);
    rlVertex3f(x + width, y + height, z - length);
    rlVertex3f(x + width, y - height, z - length);
    rlVertex3f(x - width, y - height, z - length);

    // Top Face
    rlVertex3f(x - width, y + height, z - length);
    rlVertex3f(x - width, y + height, z + length);
    rlVertex3f(x + width, y + height, z + length);
    rlVertex3f(x + width, y + height, z - length);

    // Bottom Face
    rlVertex3f(x - width, y - height, z - length);
    rlVertex3f(x + width, y - height, z - length);
    rlVertex3f(x + width, y - height, z + length);
    rlVertex3f(x - width, y - height, z + length);

    // Right Face
    rlVertex3f(x + width, y - height, z - length);
    rlVertex3f(x + width, y + height, z - length);
    rlVertex3f(x + width, y + height, z + length);
    rlVertex3f(x + width, y - height, z + length);

    // Left Face
    rlVertex3f(x - width, y - height, z - length);
    rlVertex3f(x - width, y - height, z + length);
    rlVertex3f(x - width, y + height, z + length);
    rlVertex3f(x - width, y + height, z - length);
}

Color GetFoggedColor(Color baseCol, Vector3 pos) {
    float dist = Vector3Length((Vector3){pos.x, 0, pos.z}); 
    float fogStart = 10.0f;
    float fogEnd = 300.0f; // Smoother Fade
    
    float factor = (dist - fogStart) / (fogEnd - fogStart);
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    
    return (Color){
        (unsigned char)Lerp(baseCol.r, COLOR_FOG.r, factor),
        (unsigned char)Lerp(baseCol.g, COLOR_FOG.g, factor),
        (unsigned char)Lerp(baseCol.b, COLOR_FOG.b, factor),
        255
    };
}

void LoadMenuAssets(void) {
    if (menuAssets.loaded) return;

    if (FileExists("resources/Buildings/Textures/colormap.png")) {
        menuAssets.atlas = LoadTexture("resources/Buildings/Textures/colormap.png");
        SetTextureFilter(menuAssets.atlas, TEXTURE_FILTER_BILINEAR); 
    } else {
        Image whiteImg = GenImageColor(1, 1, WHITE);
        menuAssets.atlas = LoadTextureFromImage(whiteImg);
        UnloadImage(whiteImg);
    }

    #define LOAD_MODEL(target, path) \
        target = LoadModel(path); \
        if (target.meshCount == 0) target = LoadModelFromMesh(GenMeshCube(1,1,1)); \
        target.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = menuAssets.atlas;

    LOAD_MODEL(menuAssets.light, "resources/Props/light-curved.obj");
    LOAD_MODEL(menuAssets.tree, "resources/trees/tree-small.obj");
    LOAD_MODEL(menuAssets.trash, "resources/random/trash.obj");

    // Allocate Data
    menuAssets.bPos = (Vector3*)RL_MALLOC(MAX_BG_BUILDINGS * sizeof(Vector3));
    menuAssets.bSize = (Vector3*)RL_MALLOC(MAX_BG_BUILDINGS * sizeof(Vector3));
    menuAssets.wPos = (Vector3*)RL_MALLOC(MAX_BG_WINDOWS * sizeof(Vector3));
    menuAssets.wSize = (Vector3*)RL_MALLOC(MAX_BG_WINDOWS * sizeof(Vector3));
    
    int bCount = 0;
    int wCount = 0;
    srand(42); 

    // Generate City
    for (int i = 0; i < MAX_BG_BUILDINGS; i++) {
        float x = RandF(-180.0f, 180.0f);
        float z = RandF(-300.0f, 300.0f); 
        
        if (x > -30.0f && x < 30.0f) continue; 

        bool isClose = (abs(x) < 80.0f);

        float w, d, h;
        if (isClose) {
            w = RandF(15.0f, 30.0f);
            d = RandF(15.0f, 30.0f);
            h = RandF(40.0f, 110.0f); 
        } else {
            w = RandF(10.0f, 25.0f);
            d = RandF(10.0f, 25.0f);
            h = RandF(20.0f, 60.0f); 
        }

        menuAssets.bPos[bCount] = (Vector3){ x, h/2.0f, z };
        menuAssets.bSize[bCount] = (Vector3){ w, h, d };
        bCount++;

        // Windows
        if (wCount < MAX_BG_WINDOWS - 100) { 
            int face = isClose ? 2 : GetRandomValue(0, 2);
            
            float floorH = RandF(3.5f, 4.5f);
            float colW = RandF(3.5f, 5.0f);
            float startY = isClose ? 2.5f : 15.0f;
            
            float wallWidth = (face == 2) ? d : w;
            int cols = (int)(wallWidth / colW) - 1;
            int rows = (int)((h - startY) / floorH) - 1;
            
            if (cols < 1) cols = 1; if (rows < 1) rows = 1;

            float winW = colW * 0.5f;
            float winH = floorH * 0.6f;

            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    if (wCount >= MAX_BG_WINDOWS) break;
                    if (GetRandomValue(0, 100) > (isClose ? 40 : 15)) continue;

                    float gridX = (c - (cols-1)/2.0f) * colW;
                    float gridY = startY + (r * floorH);
                    float wx, wz, wsX, wsZ;

                    if (face == 2) { 
                        if (x > 0) wx = x - w/2 - 0.2f;
                        else       wx = x + w/2 + 0.2f;
                        wz = z + gridX;
                        wsX = 0.2f; wsZ = winW;
                    } else {
                        wx = x + gridX;
                        wz = z + ((face == 0) ? d/2 + 0.2f : -d/2 - 0.2f);
                        wsX = winW; wsZ = 0.2f;
                    }

                    menuAssets.wPos[wCount] = (Vector3){ wx, gridY, wz };
                    menuAssets.wSize[wCount] = (Vector3){ wsX, winH, wsZ };
                    wCount++;
                }
            }
        }
    }
    
    // Tint Fixes
    for (int i = 0; i < menuAssets.tree.meshCount; i++) {
        if (menuAssets.tree.meshes[i].texcoords) {
            for (int v = 0; v < menuAssets.tree.meshes[i].vertexCount; v++) {
                menuAssets.tree.meshes[i].texcoords[v*2] = 200.5f/512.0f;
                menuAssets.tree.meshes[i].texcoords[v*2+1] = 400.5f/512.0f;
            }
        }
    }

    menuAssets.loaded = true;
}

void UnloadMenuAssets(void) {
    if (!menuAssets.loaded) return;
    
    // Safe Unload (Detach Textures First)
    Texture2D nullTex = { 0 }; 
    if (menuAssets.light.materialCount > 0) menuAssets.light.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = nullTex;
    if (menuAssets.tree.materialCount > 0) menuAssets.tree.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = nullTex;
    if (menuAssets.trash.materialCount > 0) menuAssets.trash.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = nullTex;

    UnloadTexture(menuAssets.atlas);
    UnloadModel(menuAssets.light);
    UnloadModel(menuAssets.tree);
    UnloadModel(menuAssets.trash);
    
    RL_FREE(menuAssets.bPos);
    RL_FREE(menuAssets.bSize);
    RL_FREE(menuAssets.wPos);
    RL_FREE(menuAssets.wSize);
    
    menuAssets.loaded = false;
}

// Static variables to persist the model between frames
static Model menuCarModel = { 0 };
static bool isMenuCarLoaded = false;

void DrawMenuPlayerCar(void) {
    // 1. Lazy Load: Run once on startup
    if (!isMenuCarLoaded) {
        char modelPath[256] = "resources/Playermodels/delivery.obj"; // Default Fallback

        // --- TRY READING SAVE FILE ---
        FILE *file = fopen("save_data.dat", "rb");
        if (file) {
            GameSaveData data;
            size_t read = fread(&data, sizeof(GameSaveData), 1, file);
            fclose(file);
            
            if (read == 1) {
                // Decrypt
                MenuObfuscate((unsigned char*)&data, sizeof(GameSaveData));
                
                // If valid save and has a custom car model
                if (data.version == SAVE_VERSION && strlen(data.modelFileName) > 0) {
                    snprintf(modelPath, 256, "resources/Playermodels/%s", data.modelFileName);
                    printf("MENU: Found player vehicle in save: %s\n", modelPath);
                }
            }
        }
        // -----------------------------

        // 2. Load the Model
        if (FileExists(modelPath)) {
            menuCarModel = LoadModel(modelPath);
            isMenuCarLoaded = true;
        } 
        else if (FileExists("resources/Playermodels/delivery.obj")) {
            // Ultimate fallback if saved mod file is missing
            menuCarModel = LoadModel("resources/Playermodels/delivery.obj");
            isMenuCarLoaded = true;
            isMenuCarLoaded = true;
        }
    }

    // 3. Draw the Model
    if (isMenuCarLoaded) {
        // Standard Transformation
        // Most models face -Z, so we rotate 180 to face camera
        Vector3 pos = { 0.0f, 0.05f, 0.0f }; 
        Vector3 axis = { 0.0f, 1.0f, 0.0f };
        float rotation = 180.0f; 
        Vector3 scale = { 1.0f, 1.0f, 1.0f }; 

        DrawModelEx(menuCarModel, pos, axis, rotation, scale, WHITE);
        
        // Simple Shadow Blob
        DrawCylinder((Vector3){0, 0.02f, 0}, 1.2f, 1.2f, 0.0f, 16, Fade(BLACK, 0.4f));
    } 
    else {
        // Red Box Error State
        DrawCube((Vector3){0, 0.5f, 0}, 1.0f, 1.0f, 2.0f, RED);
    }
}

// Static variables to persist the model between frames

void DrawMenuDiorama(void) {
    // 1. Endless Road
    for (int z = -250; z < 250; z += 50) {
        Vector3 pos = {0, -0.1f, (float)z};
        Color roadCol = GetFoggedColor((Color){15, 15, 18, 255}, pos);
        DrawCube(pos, 18.0f, 0.1f, 50.0f, roadCol);
        
        Color sideCol = GetFoggedColor((Color){60, 60, 60, 255}, pos);
        DrawCube((Vector3){10.0f, 0.05f, (float)z}, 3.0f, 0.2f, 50.0f, sideCol);
        DrawCube((Vector3){-10.0f, 0.05f, (float)z}, 3.0f, 0.2f, 50.0f, sideCol);
    }

    // Road Markings
    for (int z = 200; z > -200; z -= 8) {
        Vector3 pos = {0, 0.01f, (float)z};
        Color markCol = GetFoggedColor(WHITE, pos);
        markCol = ColorAlpha(markCol, 0.6f); 
        DrawCube(pos, 0.3f, 0.01f, 3.0f, markCol);
    }

    if (menuAssets.loaded) {
        Color treeTint = (Color){20, 50, 20, 255}; 
        
        // --- PROPS ---
        for (int z = 200; z > -200; z -= 30) {
            Vector3 pRight = {9.0f, 0.0f, (float)z};
            Vector3 pLeft = {-9.0f, 0.0f, (float)z};
            
            Color propsCol = GetFoggedColor(GRAY, pRight);
            
            // Street Lights
            DrawModelEx(menuAssets.light, pRight, (Vector3){0,1,0}, 90.0f, (Vector3){8.0f, 8.0f, 8.0f}, propsCol);
            DrawModelEx(menuAssets.light, pLeft, (Vector3){0,1,0}, -90.0f, (Vector3){8.0f, 8.0f, 8.0f}, propsCol);
            
            // Light Effects
            rlSetBlendMode(RL_BLEND_ADDITIVE);
                float bulbH = 5.2f;
                Color glowCol = GetFoggedColor(YELLOW, pRight);
                
                DrawSphere((Vector3){7.8f, bulbH, (float)z}, 0.12f, glowCol);
                DrawSphere((Vector3){-7.8f, bulbH, (float)z}, 0.12f, glowCol);
                
                Color coneCol = ColorAlpha(glowCol, 0.1f); // Fainter
                rlDisableDepthMask();
                DrawCylinderEx((Vector3){7.8f, bulbH - 0.2f, (float)z}, (Vector3){7.8f, 0.0f, (float)z}, 0.1f, 1.2f, 8, coneCol);
                DrawCylinderEx((Vector3){-7.8f, bulbH - 0.2f, (float)z}, (Vector3){-7.8f, 0.0f, (float)z}, 0.1f, 1.2f, 8, coneCol);
                rlEnableDepthMask();
            rlSetBlendMode(RL_BLEND_ALPHA); 
        }
        
        DrawModelEx(menuAssets.tree, (Vector3){12.0f, 0.0f, -5.0f}, (Vector3){0,1,0}, 45.0f, (Vector3){6.0f, 6.0f, 6.0f}, treeTint);
        DrawModelEx(menuAssets.tree, (Vector3){-12.0f, 0.0f, -15.0f}, (Vector3){0,1,0}, 90.0f, (Vector3){5.5f, 5.5f, 5.5f}, treeTint);
        DrawModelEx(menuAssets.trash, (Vector3){9.0f, 0.0f, 5.0f}, (Vector3){0,1,0}, 0.0f, (Vector3){1.5f, 1.5f, 1.5f}, GRAY);

        // --- BATCHED CITY DRAWING (HIGH PERFORMANCE) ---
        // This replaces the 6000 separate DrawCube calls
        rlBegin(RL_QUADS);
            
            // 1. Buildings
            for(int i = 0; i < MAX_BG_BUILDINGS; i++) {
                Color finalB = GetFoggedColor(COLOR_BUILDING, menuAssets.bPos[i]);
                if (finalB.a < 10) continue; 
                DrawCubeBatched(menuAssets.bPos[i], menuAssets.bSize[i], finalB);
            }

            // 2. Windows
            for(int i = 0; i < MAX_BG_WINDOWS; i++) {
                if (menuAssets.wPos[i].y == 0) continue;
                Color finalW = GetFoggedColor(COLOR_WINDOW, menuAssets.wPos[i]);
                if (finalW.a < 10) continue;
                DrawCubeBatched(menuAssets.wPos[i], menuAssets.wSize[i], finalW);
            }
            
        rlEnd();
    }
}

// --- 4. ATMOSPHERE ---
void DrawMenuAtmosphere(void) {
    rlDisableDepthMask(); 
        Color fogSolid = COLOR_FOG;
        rlBegin(RL_QUADS);
            float limit = 220.0f;
            float h = 60.0f;
            DrawCubeBatched((Vector3){0, h/2, -limit}, (Vector3){limit*2, h, 1.0f}, fogSolid);
            DrawCubeBatched((Vector3){0, h/2, limit}, (Vector3){limit*2, h, 1.0f}, fogSolid);
            DrawCubeBatched((Vector3){-limit, h/2, 0}, (Vector3){1.0f, h, limit*2}, fogSolid);
            DrawCubeBatched((Vector3){limit, h/2, 0}, (Vector3){1.0f, h, limit*2}, fogSolid);
        rlEnd();

        Color groundFog = Fade(COLOR_FOG, 0.5f); 
        DrawCube((Vector3){0, 2.5f, 0}, 450.0f, 5.0f, 450.0f, groundFog);
    rlEnableDepthMask();
}

float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

// --- LOADING UI (Scaled) ---
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status) {
    float uiScale = screenHeight / 720.0f;

    DrawRectangle(0, 0, screenWidth, screenHeight, COLOR_SKY); 

    float cx = screenWidth / 2.0f;
    float cy = screenHeight / 2.0f;
    
    float barWidth = screenWidth * 0.6f;
    if (barWidth > 800 * uiScale) barWidth = 800 * uiScale;
    float barHeight = 6.0f * uiScale;
    float barX = cx - (barWidth / 2.0f);
    float barY = cy + 80.0f * uiScale;

    int statSize = (int)(20 * uiScale);
    int statW = MeasureText(status, statSize);
    DrawText(status, cx - statW/2, barY - 30*uiScale, statSize, LIGHTGRAY);

    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, (Color){30, 30, 40, 255});
    
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;
    
    int fillW = (int)(barWidth * progress);
    DrawRectangle((int)barX, (int)barY, fillW, (int)barHeight, (Color){0, 200, 255, 255}); 
    
    if (progress > 0) DrawRectangle((int)barX + fillW - 2, (int)barY - 2, 4, (int)barHeight + 4, WHITE);

    char pctText[10];
    sprintf(pctText, "%d%%", (int)(progress * 100));
    int pctSize = (int)(20 * uiScale);
    DrawText(pctText, cx - MeasureText(pctText, pctSize)/2, barY + 20*uiScale, pctSize, DARKGRAY);
}

// ============================================================================
// PHASE 1: START MENU -> 50%
// ============================================================================
bool RunStartMenu_PreLoad(int screenWidth, int screenHeight) {
    float time = 0.0f;
    int menuState = -1; 
    
    sharedProgress = 0.0f;
    sharedStage = 0;

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = (Vector3){ 0.0f, 3.5f, 9.0f };
    camera.target = (Vector3){ 0.0f, 1.2f, 0.0f }; 

    Vector3 camStartPos = camera.position;
    Vector3 camEndPos = { 0.0f, 2.5f, -5.0f }; 
    float zoomTimer = 0.0f;

    const char* preLoadMessages[] = {
        "Initializing Physics Engine...",
        "Parsing City Graph Nodes...",
        "Generating Traffic Network...",
        "Baking Static Geometry...",
        "Optimizing Navigation Mesh...",
        "Generating Map File..."
    };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time += dt;
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float uiScale = sh / 720.0f;

        if (menuState == -1) {
            LoadMenuAssets();
            menuState = 0;
            continue; 
        }

        // --- LOGIC UPDATES ---
        
        // 1. STATE 0 (ORBIT): Check for Save File on Enter
        // 1. STATE 0: MAIN MENU (Orbit)
        if (menuState == 0) { 
            float camRadius = 12.0f;
            camera.position = (Vector3){ sinf(time * 0.15f) * camRadius, 4.5f, cosf(time * 0.15f) * camRadius };
            
            bool saveExists = FileExists(SAVE_FILE);
            
            // Auto-select New Game if no save exists
            if (!saveExists && selectedMainMenuOption == 0) selectedMainMenuOption = 1;

            // Navigation
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedMainMenuOption = 0;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedMainMenuOption = 1;
            
            // Clamp if save doesn't exist
            if (!saveExists) selectedMainMenuOption = 1;

            if (IsKeyPressed(KEY_ENTER)) {
                if (selectedMainMenuOption == 0 && saveExists) {
                    // CONTINUE
                    menuState = 1; // Go straight to Zoom/Load
                    camStartPos = camera.position;
                    zoomTimer = 0.0f;
                } 
                else if (selectedMainMenuOption == 1) {
                    // NEW GAME
                    DeleteSaveData(); // Wipe old data
                    menuState = 3;    // Go to Map Selection
                }
            }
        }
        
        // 2. [NEW] STATE 3 (MAP SELECTION)
        else if (menuState == 3) { 
            float camRadius = 12.0f;
            // Slower spin while selecting
            camera.position = (Vector3){ sinf(time * 0.05f) * camRadius, 4.5f, cosf(time * 0.05f) * camRadius }; 
            
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) selectedMapOption = 1;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) selectedMapOption = 2;
            
            if (IsKeyPressed(KEY_ENTER)) {
                SaveMapChoice(selectedMapOption); // Write config to disk
                menuState = 1; // Proceed to Zoom/Load
                camStartPos = camera.position;
                zoomTimer = 0.0f;
            }
        }
        
        // ... (State 1 and State 2 remain exactly the same) ...
        else if (menuState == 1) { // Zoom
            zoomTimer += dt;
            float t = zoomTimer / 1.5f; 
            if (t >= 1.0f) { t = 1.0f; menuState = 2; }
            float st = EaseOutCubic(t);
            camera.position.x = camStartPos.x + (camEndPos.x - camStartPos.x) * st;
            camera.position.y = camStartPos.y + (camEndPos.y - camStartPos.y) * st;
            camera.position.z = camStartPos.z + (camEndPos.z - camStartPos.z) * st;
        }
        else if (menuState == 2) { // Fake Load
            float speed = (sharedStage == 3) ? 0.3f : 0.9f; 
            sharedProgress += speed * dt;
            if (sharedProgress > 0.10f) sharedStage = 1;
            if (sharedProgress > 0.20f) sharedStage = 2;
            if (sharedProgress > 0.30f) sharedStage = 3;
            if (sharedProgress > 0.45f) sharedStage = 4;
            
            if (sharedProgress >= 0.50f) {
                sharedProgress = 0.50f;
                sharedStage = 5; 
                static int f = 0; f++;
                if (f > 10) { UnloadMenuAssets(); return true; }
            }
        }

        // --- DRAWING ---
        BeginDrawing();
            
            if (menuState <= 1 || menuState == 3) {
                ClearBackground(COLOR_FOG); 
                BeginMode3D(camera);
                    DrawMenuDiorama();      
                    DrawMenuPlayerCar();      
                    DrawMenuAtmosphere();   
                EndMode3D();
                
                if (menuState == 0) {
                    float boxW = 400 * uiScale;
                    float boxH = 70 * uiScale;
                    DrawRectangle(20*uiScale, 20*uiScale, boxW, boxH, Fade(BLACK, 0.7f));
                    DrawRectangleLines(20*uiScale, 20*uiScale, boxW, boxH, Fade(WHITE, 0.3f));
                    
                    DrawText("AUTH ECE - COURSE 004", 35*uiScale, 30*uiScale, 20*uiScale, WHITE);
                    DrawText("Structured Programming", 35*uiScale, 55*uiScale, 12*uiScale, LIGHTGRAY);

                    int titleSize = (int)(60 * uiScale);
                    const char* title = "DELIVERY GAME 3D";
                    int tw = MeasureText(title, titleSize);
                    DrawText(title, (sw - tw)/2 + 4, sh * 0.18f + 4, titleSize, Fade(BLACK, 0.5f));
                    DrawText(title, (sw - tw)/2, sh * 0.18f, titleSize, YELLOW);
                    
                    const char* authors = "CREATED BY: MICHAIL MICHAILIDIS & LUCAS LICO";
                    int aw = MeasureText(authors, (int)(22*uiScale));
                    DrawText(authors, (sw - aw)/2, sh * 0.18f + titleSize + 15*uiScale, (int)(22*uiScale), SKYBLUE);

                    bool saveExists = FileExists(SAVE_FILE);
                    
                    int btnW = (int)(300 * uiScale);
                    int btnH = (int)(60 * uiScale);
                    int startY = (int)(sh * 0.75f);
                    int spacing = (int)(20 * uiScale);
                    int centerX = (sw - btnW) / 2;

                    // --- CONTINUE BUTTON ---
                    Color contColor = saveExists ? WHITE : GRAY;
                    Color contBg = (selectedMainMenuOption == 0 && saveExists) ? (Color){0, 100, 0, 200} : (Color){20, 20, 20, 200};
                    Color contBorder = (selectedMainMenuOption == 0 && saveExists) ? LIME : GRAY;
                    
                    DrawRectangle(centerX, startY, btnW, btnH, contBg);
                    DrawRectangleLinesEx((Rectangle){centerX, startY, btnW, btnH}, 2, contBorder);
                    
                    const char* txtCont = "CONTINUE";
                    int cw = MeasureText(txtCont, (int)(24*uiScale));
                    DrawText(txtCont, centerX + (btnW - cw)/2, startY + (btnH/2) - (12*uiScale), (int)(24*uiScale), contColor);
                    
                    if (!saveExists) {
                        DrawLine(centerX + 20, startY + btnH/2, centerX + btnW - 20, startY + btnH/2, GRAY);
                    }

                    // --- NEW GAME BUTTON ---
                    int ngY = startY + btnH + spacing;
                    Color ngBg = (selectedMainMenuOption == 1) ? (Color){100, 0, 0, 200} : (Color){20, 20, 20, 200};
                    Color ngBorder = (selectedMainMenuOption == 1) ? RED : GRAY;

                    DrawRectangle(centerX, ngY, btnW, btnH, ngBg);
                    DrawRectangleLinesEx((Rectangle){centerX, ngY, btnW, btnH}, 2, ngBorder);

                    const char* txtNew = "NEW GAME";
                    int nw = MeasureText(txtNew, (int)(24*uiScale));
                    DrawText(txtNew, centerX + (btnW - nw)/2, ngY + (btnH/2) - (12*uiScale), (int)(24*uiScale), WHITE);
                    
                    // Controls hint
                    const char* hint = "Use W/S or UP/DOWN to Select, ENTER to Confirm";
                    int hw = MeasureText(hint, (int)(16*uiScale));
                    DrawText(hint, (sw - hw)/2, sh - (34*uiScale), (int)(16*uiScale), LIGHTGRAY);

                    const char* copy = "v1.0 (2026) | License: zlib/libpng";
                    int copyw = MeasureText(copy, (int)(12*uiScale));
                    DrawText(copy, sw - copyw - 20*uiScale, sh - 20*uiScale, (int)(12*uiScale), DARKGRAY);
                }
                
                // [NEW] Draw Map Selection UI
                if (menuState == 3) {
                    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.85f)); // Dark Overlay
                    
                    int titleSize = (int)(40 * uiScale);
                    const char* title = "SELECT CITY SIZE";
                    DrawText(title, (sw - MeasureText(title, titleSize))/2, sh * 0.15f, titleSize, WHITE);
                    
                    int cardW = (int)(sw * 0.35f);
                    int cardH = (int)(sh * 0.4f);
                    int gap = (int)(sw * 0.05f);
                    int startX = (sw - (cardW*2 + gap)) / 2;
                    int startY = (int)(sh * 0.3f);
                    
                    // Option 1: Small City
                    DrawSelectionBox(startX, startY, cardW, cardH, 
                        "SMALL CITY", 
                        "Optimized Performance\nCompact Layout\nBest for Laptops", 
                        (selectedMapOption == 1));
                        
                    // Option 2: Big City
                    DrawSelectionBox(startX + cardW + gap, startY, cardW, cardH, 
                        "METROPOLIS", 
                        "Complex Traffic\nExpansive World\nNeeds Good GPU", 
                        (selectedMapOption == 2));
                        
                    // Instructions
                    const char* instr = "Use ARROW KEYS to Select, ENTER to Confirm";
                    DrawText(instr, (sw - MeasureText(instr, 20))/2, sh * 0.85f, 20, LIGHTGRAY);
                }

                if (menuState == 1) {
                    float zoomT = zoomTimer / 1.5f; 
                    if (zoomT > 0.6f) {
                        float fadeAlpha = (zoomT - 0.6f) / 0.4f;
                        if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;
                        DrawRectangle(0, 0, sw, sh, Fade(COLOR_SKY, fadeAlpha));
                    }
                }
            } else {
                DrawLoadingInterface(sw, sh, sharedProgress, preLoadMessages[sharedStage]);
            }
            
        EndDrawing();
    }
    // At the end of RunStartMenu_PreLoad before return:
if (isMenuCarLoaded) {
    UnloadModel(menuCarModel);
    isMenuCarLoaded = false;
}
    return false;
}

bool DrawPostLoadOverlay(int screenWidth, int screenHeight, float dt) {
    if (sharedProgress < 0.5f) sharedProgress = 0.5f;

    const char* postLoadMessages[] = {
        "Loading Map File...", 
        "Generating Terrain Chunks...", 
        "Populating City Sectors...", 
        "Spawning AI Traffic...", 
        "Igniting Engine...",
        "Ready"
    };

    int msgIndex = 0;
    if (sharedProgress > 0.6f) msgIndex = 1;
    if (sharedProgress > 0.7f) msgIndex = 2;
    if (sharedProgress > 0.8f) msgIndex = 3;
    if (sharedProgress > 0.9f) msgIndex = 4;
    if (sharedProgress >= 1.0f) msgIndex = 5;

    sharedProgress += 0.06f * dt; 

    DrawLoadingInterface(screenWidth, screenHeight, sharedProgress, postLoadMessages[msgIndex]);

    if (sharedProgress >= 1.0f) {
        return false; 
    }
    return true; 
}
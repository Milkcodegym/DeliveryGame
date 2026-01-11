#include "start_menu.h" 
#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- SHARED STATE ---
// We keep these static so state persists between the two functions
static float sharedProgress = 0.0f;
static int sharedStage = 0;

// --- 1. LOCAL ASSET MANAGEMENT ---
typedef struct {
    Texture2D atlas;
    Model light;
    Model tree;
    Model trash;
    Model building_tall;
    Model building_wide;
    bool loaded;
} MenuAssets;

static MenuAssets menuAssets = {0};

void Menu_SetMeshUVs(Mesh *mesh, float u, float v) {
    if (mesh->texcoords == NULL) return;
    for (int i = 0; i < mesh->vertexCount; i++) {
        mesh->texcoords[i*2] = u;
        mesh->texcoords[i*2+1] = v;
    }
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

    #define LOAD_MENU_MODEL(target, path) \
        target = LoadModel(path); \
        if (target.meshCount == 0) target = LoadModelFromMesh(GenMeshCube(1,1,1)); \
        target.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = menuAssets.atlas;

    LOAD_MENU_MODEL(menuAssets.light, "resources/Props/light-curved.obj");
    LOAD_MENU_MODEL(menuAssets.tree, "resources/trees/tree-small.obj");
    LOAD_MENU_MODEL(menuAssets.trash, "resources/random/trash.obj");
    LOAD_MENU_MODEL(menuAssets.building_tall, "resources/Buildings/windows_tall.obj");
    LOAD_MENU_MODEL(menuAssets.building_wide, "resources/Buildings/Windows_detailed.obj");

    float atlasW = 512.0f; 
    float atlasH = 512.0f;
    float whitePixelU = (200.0f + 0.5f) / atlasW;
    float whitePixelV = (400.0f + 0.5f) / atlasH;

    Menu_SetMeshUVs(&menuAssets.tree.meshes[0], whitePixelU, whitePixelV);
    Menu_SetMeshUVs(&menuAssets.trash.meshes[0], whitePixelU, whitePixelV);
    
    menuAssets.loaded = true;
}

void UnloadMenuAssets(void) {
    if (!menuAssets.loaded) return;
    UnloadTexture(menuAssets.atlas);
    UnloadModel(menuAssets.light);
    UnloadModel(menuAssets.tree);
    UnloadModel(menuAssets.trash);
    UnloadModel(menuAssets.building_tall);
    UnloadModel(menuAssets.building_wide);
    menuAssets.loaded = false;
}

// --- 2. DRAW VESPA ---
void DrawSimpleVespa(void) {
    Color bodyColor = RED; 
    Color seatColor = BROWN;
    Color tireColor = DARKGRAY;
    Color metalColor = LIGHTGRAY;

    DrawCube((Vector3){0, 0.6f, -0.5f}, 0.8f, 0.7f, 1.4f, bodyColor);
    DrawCube((Vector3){0, 0.3f, 0.5f}, 0.8f, 0.1f, 1.2f, bodyColor);
    DrawCube((Vector3){0, 0.8f, 1.2f}, 0.8f, 1.2f, 0.1f, bodyColor);
    DrawCube((Vector3){0, 1.0f, -0.7f}, 0.7f, 0.15f, 0.7f, seatColor);
    DrawLine3D((Vector3){0, 0.3f, 1.5f}, (Vector3){0, 1.6f, 1.1f}, metalColor);
    DrawCube((Vector3){0, 1.6f, 1.1f}, 1.2f, 0.1f, 0.1f, metalColor);
    DrawSphere((Vector3){0, 1.6f, 1.2f}, 0.15f, RAYWHITE);
    DrawCylinder((Vector3){0, 0.3f, -1.0f}, 0.3f, 0.3f, 0.2f, 16, tireColor);
    DrawCylinderWires((Vector3){0, 0.3f, -1.0f}, 0.3f, 0.3f, 0.2f, 16, BLACK);
    DrawCylinder((Vector3){0, 0.3f, 1.5f}, 0.3f, 0.3f, 0.2f, 16, tireColor);
    DrawCylinderWires((Vector3){0, 0.3f, 1.5f}, 0.3f, 0.3f, 0.2f, 16, BLACK);
    DrawCylinder((Vector3){0, 0.01f, 0.2f}, 0.8f, 0.8f, 0.0f, 8, Fade(BLACK, 0.5f));
    DrawLine3D((Vector3){-0.2f, 0.3f, 0.0f}, (Vector3){-0.5f, 0.0f, 0.0f}, LIGHTGRAY);
}

// --- 3. DRAW ENVIRONMENT ---
void DrawMenuDiorama(void) {
    DrawCube((Vector3){0, -0.05f, 0}, 20.0f, 0.1f, 20.0f, (Color){40, 40, 40, 255});
    DrawCube((Vector3){3.5f, 0.05f, 0}, 3.0f, 0.2f, 20.0f, LIGHTGRAY);

    if (menuAssets.loaded) {
        DrawModelEx(menuAssets.light, (Vector3){2.5f, 0.0f, 2.0f}, (Vector3){0,1,0}, -90.0f, (Vector3){2.8f, 2.8f, 2.8f}, WHITE);
        DrawSphere((Vector3){1.0f, 5.5f, 2.0f}, 0.2f, YELLOW); 
        
        Color treeTint = (Color){40, 110, 40, 255}; 
        DrawModelEx(menuAssets.tree, (Vector3){4.0f, 0.0f, -3.0f}, (Vector3){0,1,0}, 45.0f, (Vector3){5.0f, 5.0f, 5.0f}, treeTint);
        DrawModelEx(menuAssets.trash, (Vector3){2.8f, 0.0f, -0.5f}, (Vector3){0,1,0}, 10.0f, (Vector3){1.2f, 1.2f, 1.2f}, GRAY);

        Color bldgTint = (Color){80, 80, 90, 255}; 
        DrawModelEx(menuAssets.building_tall, (Vector3){10.0f, 0.0f, 5.0f}, (Vector3){0,1,0}, -90.0f, (Vector3){2.0f, 10.0f, 2.0f}, bldgTint);
        DrawModelEx(menuAssets.building_wide, (Vector3){-12.0f, 0.0f, 8.0f}, (Vector3){0,1,0}, 90.0f, (Vector3){2.0f, 5.0f, 4.0f}, bldgTint);
    }
}

float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

// --- DRAWING HELPER ---
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status) {
    // Background - Slightly Transparent so we can see the game spawning behind it in Phase 2
    DrawRectangle(0, 0, screenWidth, screenHeight, GetColor(0x151520FF)); 

    float cx = screenWidth / 2.0f;
    float cy = screenHeight / 2.0f;
    
    float barWidth = screenWidth * 0.6f;
    if (barWidth > 600) barWidth = 600;
    if (barWidth < 300) barWidth = 300;
    
    float barHeight = 24.0f;
    float barX = cx - (barWidth / 2.0f);
    float barY = cy + 100.0f;

    const char* title = "LOADING CITY DATA";
    int titleSize = 28;
    int titleW = MeasureText(title, titleSize);
    DrawText(title, cx - titleW/2, barY - 50, titleSize, WHITE);

    DrawRectangleLinesEx((Rectangle){barX, barY, barWidth, barHeight}, 2.0f, DARKGRAY);
    
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;
    
    float time = GetTime();
    BeginScissorMode((int)barX + 2, (int)barY + 2, (int)((barWidth - 4) * progress), (int)barHeight - 4);
        DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGREEN);
        for (int i = 0; i < (int)(barWidth/20) + 2; i++) {
            float xPos = barX + (i * 40) + (time * 60); 
            float localX = fmodf(xPos - barX, barWidth + 40); 
            DrawRectangle((int)(barX + localX - 40), (int)barY, 15, (int)barHeight, LIME);
        }
    EndScissorMode();

    int dotCount = (int)(time * 3.0f) % 4;
    char dotStr[5] = "";
    for(int i=0; i<dotCount; i++) strcat(dotStr, ".");
    
    char fullStatus[128];
    snprintf(fullStatus, 128, "%s%s", status, dotStr);
    
    int statSize = 20;
    int statW = MeasureText(fullStatus, statSize);
    DrawText(fullStatus, cx - statW/2, barY + 35, statSize, LIGHTGRAY);

    DrawRectangleGradientV(0, 0, screenWidth, screenHeight/4, BLACK, BLANK);
    DrawRectangleGradientV(0, screenHeight - screenHeight/4, screenWidth, screenHeight/4, BLANK, BLACK);
}

// ============================================================================
// PHASE 1: START MENU -> 50%
// ============================================================================
bool RunStartMenu_PreLoad(int screenWidth, int screenHeight) {
    float time = 0.0f;
    int menuState = -1; 
    
    // Reset Shared State
    sharedProgress = 0.0f;
    sharedStage = 0;

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 camStartPos = {0};
    Vector3 camEndPos = { 0.0f, 2.5f, -5.0f }; 
    Vector3 camCurrentTarget = { 0.0f, 0.8f, 0.0f }; 
    float zoomTimer = 0.0f;

    const char* preLoadMessages[] = {
        "Initializing Physics Engine",  
        "Parsing City Graph",           
        "Generating Traffic Nodes",     
        "Baking Static Geometry",       
        "Calculating Navigation Mesh",  
        "Initializing Map File..."
    };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time += dt;
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        if (menuState == -1) {
            LoadMenuAssets();
            menuState = 0;
            continue;
        }

        if (menuState == 0) { // IDLE
            float camRadius = 9.0f;
            camera.position = (Vector3){ sinf(time * 0.3f) * camRadius, 3.5f, cosf(time * 0.3f) * camRadius };
            camera.target = camCurrentTarget;
            if (IsKeyPressed(KEY_ENTER)) {
                menuState = 1;
                camStartPos = camera.position;
                zoomTimer = 0.0f;
            }
        }
        else if (menuState == 1) { // ZOOM
            zoomTimer += dt;
            float t = zoomTimer / 1.5f; 
            if (t >= 1.0f) { t = 1.0f; menuState = 2; }
            camera.position.x = camStartPos.x + (camEndPos.x - camStartPos.x) * EaseOutCubic(t);
            camera.position.y = camStartPos.y + (camEndPos.y - camStartPos.y) * EaseOutCubic(t);
            camera.position.z = camStartPos.z + (camEndPos.z - camStartPos.z) * EaseOutCubic(t);
        }
        else if (menuState == 2) { // FAKE LOAD 0% -> 50%
            float speed = 0.0f;
            if (sharedStage == 0) speed = 0.8f;
            else if (sharedStage == 3) speed = 0.2f; 
            else speed = 0.5f;

            sharedProgress += speed * dt;
            
            // Advance fake stages
            if (sharedProgress > 0.10f) sharedStage = 1;
            if (sharedProgress > 0.20f) sharedStage = 2;
            if (sharedProgress > 0.30f) sharedStage = 3;
            if (sharedProgress > 0.40f) sharedStage = 4;
            
            // STOP AT 50%
            if (sharedProgress >= 0.50f) {
                sharedProgress = 0.50f;
                sharedStage = 5; // "Initializing Map File..."
                
                // Force a few frames of drawing the 50% state so the user sees it
                // before we return and freeze for the Real Load.
                static int bufferFrames = 0;
                bufferFrames++;
                if (bufferFrames > 5) {
                    UnloadMenuAssets(); // Free memory for the map
                    return true; // SIGNAL TO LOAD REAL MAP
                }
            }
        }

        BeginDrawing();
            if (menuState <= 1) {
                ClearBackground((Color){15, 15, 30, 255}); 
                BeginMode3D(camera);
                    DrawMenuDiorama();
                    DrawSimpleVespa();
                EndMode3D();
                
                // UI (Title/Press Start) logic (Simulated here for brevity, same as before)
                if (menuState == 0) {
                    DrawText("DELIVERY GAME 3D", (sw - MeasureText("DELIVERY GAME 3D", 40))/2, sh * 0.15f, 40, YELLOW);
                    if ((int)(time*2)%2==0) DrawText("[ PRESS ENTER ]", (sw - MeasureText("[ PRESS ENTER ]", 20))/2, sh * 0.85f, 20, WHITE);
                }
            } else {
                DrawLoadingInterface(sw, sh, sharedProgress, preLoadMessages[sharedStage]);
            }
        EndDrawing();
    }
    
    return false; // Window closed
}

// ============================================================================
// PHASE 2: OVERLAY 50% -> 100%
// ============================================================================
bool DrawPostLoadOverlay(int screenWidth, int screenHeight, float dt) {
    // Continue from 50%
    if (sharedProgress < 0.5f) sharedProgress = 0.5f;

    // Fake stages for post-load
    const char* postLoadMessages[] = {
        "Initializing Map File...", // < 0.6
        "Generating Chunks",        // 0.6 - 0.7
        "Populating Sectors",       // 0.7 - 0.8
        "Spawning Traffic",         // 0.8 - 0.9
        "Starting Engine",          // 0.9 - 1.0
        "Ready"
    };

    // Determine current message based on progress
    int msgIndex = 0;
    if (sharedProgress > 0.6f) msgIndex = 1;
    if (sharedProgress > 0.7f) msgIndex = 2;
    if (sharedProgress > 0.8f) msgIndex = 3;
    if (sharedProgress > 0.9f) msgIndex = 4;
    if (sharedProgress >= 1.0f) msgIndex = 5;

    // Slow, steady progress (Take ~3 seconds)
    // 0.5 remaining / 0.15 speed = ~3.3 seconds
    sharedProgress += 0.1f * dt; 

    // Draw the overlay ON TOP of the game
    DrawLoadingInterface(screenWidth, screenHeight, sharedProgress, postLoadMessages[msgIndex]);

    if (sharedProgress >= 1.0f) {
        return false; // Done!
    }
    return true; // Keep drawing
}
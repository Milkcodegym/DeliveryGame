#include "start_menu.h" // Only includes function prototypes, no map logic
#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 1. LOCAL ASSET MANAGEMENT ---
// We define a small struct to hold ONLY the assets needed for the menu
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

// Helper from map.c: Collapses UVs to a single point (to allow coloring/tinting)
void Menu_SetMeshUVs(Mesh *mesh, float u, float v) {
    if (mesh->texcoords == NULL) return;
    for (int i = 0; i < mesh->vertexCount; i++) {
        mesh->texcoords[i*2] = u;
        mesh->texcoords[i*2+1] = v;
    }
}

// Replicate logic from map.c LoadCityAssets
void LoadMenuAssets(void) {
    if (menuAssets.loaded) return;

    // 1. Load the shared texture (Atlas)
    if (FileExists("resources/Buildings/Textures/colormap.png")) {
        menuAssets.atlas = LoadTexture("resources/Buildings/Textures/colormap.png");
        SetTextureFilter(menuAssets.atlas, TEXTURE_FILTER_BILINEAR); 
    } else {
        // Fallback if file missing
        Image whiteImg = GenImageColor(1, 1, WHITE);
        menuAssets.atlas = LoadTextureFromImage(whiteImg);
        UnloadImage(whiteImg);
    }

    // 2. Load Specific Models (Using paths from map.c)
    // We add a helper macro to keep it clean
    #define LOAD_MENU_MODEL(target, path) \
        target = LoadModel(path); \
        if (target.meshCount == 0) target = LoadModelFromMesh(GenMeshCube(1,1,1)); \
        target.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = menuAssets.atlas;

    LOAD_MENU_MODEL(menuAssets.light, "resources/Props/light-curved.obj");
    LOAD_MENU_MODEL(menuAssets.tree, "resources/trees/tree-small.obj");
    LOAD_MENU_MODEL(menuAssets.trash, "resources/random/trash.obj");
    LOAD_MENU_MODEL(menuAssets.building_tall, "resources/Buildings/windows_tall.obj");
    LOAD_MENU_MODEL(menuAssets.building_wide, "resources/Buildings/Windows_detailed.obj");

    // 3. Apply the "Tinting Fix"
    // In map.c, you mapped specific props to a white pixel so they can be colored.
    // We do the same here for the tree and trash so they aren't just white/textured.
    float atlasW = 512.0f; 
    float atlasH = 512.0f;
    float whitePixelU = (200.0f + 0.5f) / atlasW;
    float whitePixelV = (400.0f + 0.5f) / atlasH;

    Menu_SetMeshUVs(&menuAssets.tree.meshes[0], whitePixelU, whitePixelV);
    Menu_SetMeshUVs(&menuAssets.trash.meshes[0], whitePixelU, whitePixelV);
    
    // Note: Buildings usually keep their original UVs, so we don't touch them.

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

// --- 2. DRAW VESPA (Immobile + Kickstand) ---
void DrawSimpleVespa(void) {
    Color bodyColor = RED; 
    Color seatColor = BROWN;
    Color tireColor = DARKGRAY;
    Color metalColor = LIGHTGRAY;

    // Body
    DrawCube((Vector3){0, 0.6f, -0.5f}, 0.8f, 0.7f, 1.4f, bodyColor);
    DrawCube((Vector3){0, 0.3f, 0.5f}, 0.8f, 0.1f, 1.2f, bodyColor);
    DrawCube((Vector3){0, 0.8f, 1.2f}, 0.8f, 1.2f, 0.1f, bodyColor);
    
    // Seat
    DrawCube((Vector3){0, 1.0f, -0.7f}, 0.7f, 0.15f, 0.7f, seatColor);

    // Handlebars
    DrawLine3D((Vector3){0, 0.3f, 1.5f}, (Vector3){0, 1.6f, 1.1f}, metalColor);
    DrawCube((Vector3){0, 1.6f, 1.1f}, 1.2f, 0.1f, 0.1f, metalColor);
    DrawSphere((Vector3){0, 1.6f, 1.2f}, 0.15f, RAYWHITE);

    // Wheels
    DrawCylinder((Vector3){0, 0.3f, -1.0f}, 0.3f, 0.3f, 0.2f, 16, tireColor);
    DrawCylinderWires((Vector3){0, 0.3f, -1.0f}, 0.3f, 0.3f, 0.2f, 16, BLACK);
    DrawCylinder((Vector3){0, 0.3f, 1.5f}, 0.3f, 0.3f, 0.2f, 16, tireColor);
    DrawCylinderWires((Vector3){0, 0.3f, 1.5f}, 0.3f, 0.3f, 0.2f, 16, BLACK);

    // Shadow
    DrawCylinder((Vector3){0, 0.01f, 0.2f}, 0.8f, 0.8f, 0.0f, 8, Fade(BLACK, 0.5f));

    // Kickstand (Visualizes that it is parked)
    DrawLine3D((Vector3){-0.2f, 0.3f, 0.0f}, (Vector3){-0.5f, 0.0f, 0.0f}, LIGHTGRAY);
}

// --- 3. DRAW ENVIRONMENT (The Diorama) ---
void DrawMenuDiorama(void) {
    // 1. Static Floor (Asphalt)
    DrawCube((Vector3){0, -0.05f, 0}, 20.0f, 0.1f, 20.0f, (Color){40, 40, 40, 255});
    
    // 2. Sidewalk Curb (Right side)
    DrawCube((Vector3){3.5f, 0.05f, 0}, 3.0f, 0.2f, 20.0f, LIGHTGRAY);

    if (menuAssets.loaded) {
        // 3. Street Light (Casting light on the bike)
        // Note: Using -90 rotation to face the road, similar to BakeRoadDetails logic
        DrawModelEx(menuAssets.light, (Vector3){2.5f, 0.0f, 2.0f}, (Vector3){0,1,0}, -90.0f, (Vector3){2.8f, 2.8f, 2.8f}, WHITE);
        
        // Fake Glow
        DrawSphere((Vector3){1.0f, 5.5f, 2.0f}, 0.2f, YELLOW);

        // 4. Tree (Back Right)
        // Using the same tint color logic as GenerateParkFoliage inside map.c
        Color treeTint = (Color){40, 110, 40, 255}; 
        DrawModelEx(menuAssets.tree, (Vector3){4.0f, 0.0f, -3.0f}, (Vector3){0,1,0}, 45.0f, (Vector3){5.0f, 5.0f, 5.0f}, treeTint);

        // 5. Trash Can (Detail)
        DrawModelEx(menuAssets.trash, (Vector3){2.8f, 0.0f, -0.5f}, (Vector3){0,1,0}, 10.0f, (Vector3){1.2f, 1.2f, 1.2f}, GRAY);

        // 6. Background Buildings (Silhouettes)
        Color bldgTint = (Color){80, 80, 90, 255}; 
        
        // Tall one far back
        DrawModelEx(menuAssets.building_tall, (Vector3){10.0f, 0.0f, 5.0f}, (Vector3){0,1,0}, -90.0f, (Vector3){2.0f, 10.0f, 2.0f}, bldgTint);
        
        // Wide one far back left
        DrawModelEx(menuAssets.building_wide, (Vector3){-12.0f, 0.0f, 8.0f}, (Vector3){0,1,0}, 90.0f, (Vector3){2.0f, 5.0f, 4.0f}, bldgTint);
    }
}

// --- 4. MATH HELPER ---
float EaseOutCubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

// --- 5. MAIN MENU LOOP ---
GameMap RunStartMenu(const char* mapFileName, int screenWidth, int screenHeight) {
    float time = 0.0f;
    int menuState = -1; // -1 = Load, 0 = Idle, 1 = Zoom, 2 = Done
    GameMap map = {0}; 

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Zoom setup
    Vector3 camStartPos = {0};
    Vector3 camEndPos = { 0.0f, 2.5f, -5.0f }; // Behind bike
    Vector3 camCurrentTarget = { 0.0f, 0.8f, 0.0f }; 
    float zoomTimer = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time += dt;

        // --- LOADING PHASE ---
        if (menuState == -1) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("LOADING ASSETS...", 50, screenHeight - 50, 20, WHITE);
            EndDrawing();

            LoadMenuAssets(); // Load our local menu assets
            menuState = 0;
            continue;
        }

        // --- UPDATE ---
        if (menuState == 0) {
            // Idle Orbit
            float camRadius = 9.0f;
            float camX = sinf(time * 0.3f) * camRadius;
            float camZ = cosf(time * 0.3f) * camRadius;
            camera.position = (Vector3){ camX, 3.5f, camZ };
            camera.target = camCurrentTarget;

            if (IsKeyPressed(KEY_ENTER)) {
                menuState = 1;
                camStartPos = camera.position;
                zoomTimer = 0.0f;
            }
        }
        else if (menuState == 1) {
            // Zoom In
            zoomTimer += dt;
            float t = zoomTimer / 2.0f; // 2.0s duration
            if (t >= 1.0f) { t = 1.0f; menuState = 2; }
            float smoothT = EaseOutCubic(t);

            camera.position.x = camStartPos.x + (camEndPos.x - camStartPos.x) * smoothT;
            camera.position.y = camStartPos.y + (camEndPos.y - camStartPos.y) * smoothT;
            camera.position.z = camStartPos.z + (camEndPos.z - camStartPos.z) * smoothT;
        }

        // --- DRAW ---
        BeginDrawing();
            // Night Sky
            ClearBackground((Color){15, 15, 30, 255}); 

            BeginMode3D(camera);
                DrawMenuDiorama();
                DrawSimpleVespa();
            EndMode3D();

            // UI
            if (menuState == 0) {
                DrawText("DELIVERY GAME 3D", 50, 50, 80, YELLOW);
                DrawText("v 0.5.1 // Raylib // @MILK @LITSOLOU19", 55, 130, 20, GRAY);
                
                float alpha = (sinf(time * 4.0f) + 1.0f) * 0.5f; 
                DrawText("[ PRESS ENTER ]", screenWidth/2 - 80, screenHeight - 100, 20, Fade(WHITE, alpha));
            }
            else if (menuState == 1) {
                float alpha = 1.0f - (zoomTimer / 0.5f);
                if (alpha > 0) DrawText("DELIVERY GAME 3D", 50, 50, 80, Fade(YELLOW, alpha));
            }
            else if (menuState == 2) {
                // Static Loading Bar before actual load
                // This is purely visual
                int barW = 400; int barH = 20;
                int bx = (screenWidth - barW)/2; int by = screenHeight/2 + 100;
                DrawRectangle(bx, by, barW, barH, DARKGREEN);
                DrawText("Loading City Map...", bx, by - 30, 20, WHITE);
            }
        EndDrawing();

        // --- TRANSITION TO GAME ---
        if (menuState == 2) {
            // Force one last draw so the user sees the "Loading..." text
            // Then execute the heavy load
            UnloadMenuAssets(); // Clean up menu memory
            map = LoadGameMap(mapFileName); // External function (from start_menu.h / main.c)
            break;
        }
    }
    return map;
}

// --- SHARED DRAWING FUNCTION ---
void DrawLoadingInterface(int screenWidth, int screenHeight, float progress, const char* status) {
    // 1. Draw a dark background to cover whatever is behind
    DrawRectangle(0, 0, screenWidth, screenHeight, GetColor(0x202020FF)); 

    // 2. Moving stripes (Visual feedback)
    float time = GetTime();
    int scrollOffset = (int)(time * 100) % 80; 
    for (int i = -1; i < screenHeight / 60 + 1; i++) {
        DrawRectangle(screenWidth / 2 - 10, (i * 80) + scrollOffset, 20, 40, YELLOW);
    }

    // 3. The Bar
    int barWidth = 400;
    int barHeight = 30;
    int barX = (screenWidth - barWidth) / 2;
    int barY = screenHeight / 2;

    DrawText("LOADING CITY...", barX, barY - 40, 20, WHITE);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    
    // Clamp progress
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;

    DrawRectangle(barX + 5, barY + 5, (int)((barWidth - 10) * progress), barHeight - 10, GREEN);
    DrawText(status, barX, barY + 40, 20, LIGHTGRAY);
}
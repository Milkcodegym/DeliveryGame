/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 * License: zlib/libpng
 * -----------------------------------------------------------------------------
 */

#include "dealership.h"
#include "raymath.h"
#include "player.h" // [FIX] Required for Player struct access
#include <stdio.h>
#include <string.h>

// --- Configuration ---
#define MAX_CARS 7
#define DEALERSHIP_LOCATION (Vector3){ 50.0f, 0.0f, 50.0f } 
#define TRIGGER_RADIUS 5.0f

// --- Internal Structures ---

typedef struct CarStats {
    float maxSpeed;
    float acceleration;
    float brakePower;
    float price;
    
    // [NEW] Physics Handling
    float turnSpeed;
    float friction;
    float drag;

    // Utility & Mechanics
    float maxFuel;          
    float fuelConsumption;  
    float insulation;       
    float loadSensitivity;  

    char name[32];
    char modelFileName[64];
} CarStats;

typedef struct CarEntry {
    CarStats base;
    CarStats upgrade;
    bool hasUpgrade;
    Model modelBase;
    Model modelUpgrade;
} CarEntry;

// --- Globals ---

static Model tableModel;
static Model screenModel;
static Model wallModel;
static Model systemModel;
static Model coneModel;
static Model railModel;
static Model containerModel;
static Model containerOpenModel;
static Model skipModel;
static Model floorPanelModel;

static Camera3D shopCamera;
static CarEntry carDatabase[MAX_CARS];
static int currentSelection = 0;
static bool viewingUpgrade = false;
static float carRotation = 0.0f;
static DealershipState currentState = DEALERSHIP_INACTIVE;

// --- Database Population ---
void InitCarDatabase() {
    // 0. Delivery Van (Balanced)
    strcpy(carDatabase[0].base.name, "Delivery Van");
    strcpy(carDatabase[0].base.modelFileName, "delivery.obj");
    carDatabase[0].base.maxSpeed = 60.0f; 
    carDatabase[0].base.acceleration = 0.3f;
    carDatabase[0].base.brakePower = 2.0f;
    carDatabase[0].base.turnSpeed = 2.5f;   // Standard
    carDatabase[0].base.friction = 0.98f;
    carDatabase[0].base.drag = 0.02f;
    carDatabase[0].base.price = 500.0f;
    carDatabase[0].base.maxFuel = 80.0f;       
    carDatabase[0].base.fuelConsumption = 0.04f; 
    carDatabase[0].base.insulation = 0.7f;     
    carDatabase[0].base.loadSensitivity = 0.3f; 
    carDatabase[0].hasUpgrade = false;

    // 1. Sedan (Agile)
    strcpy(carDatabase[1].base.name, "Sedan Standard");
    strcpy(carDatabase[1].base.modelFileName, "sedan.obj");
    carDatabase[1].base.maxSpeed = 80.0f; 
    carDatabase[1].base.acceleration = 0.5f;
    carDatabase[1].base.brakePower = 2.5f;
    carDatabase[1].base.turnSpeed = 3.0f;   // Sharper
    carDatabase[1].base.friction = 0.98f;
    carDatabase[1].base.drag = 0.015f;      // Aerodynamic
    carDatabase[1].base.price = 1500.0f;
    carDatabase[1].base.maxFuel = 50.0f;
    carDatabase[1].base.fuelConsumption = 0.02f; 
    carDatabase[1].base.insulation = 0.9f;     
    carDatabase[1].base.loadSensitivity = 0.8f; 
    carDatabase[1].hasUpgrade = true;

    strcpy(carDatabase[1].upgrade.name, "Sedan Sport");
    strcpy(carDatabase[1].upgrade.modelFileName, "sedan-sports.obj");
    carDatabase[1].upgrade.maxSpeed = 110.0f;
    carDatabase[1].upgrade.acceleration = 0.7f;
    carDatabase[1].upgrade.brakePower = 3.0f;
    carDatabase[1].upgrade.turnSpeed = 3.2f;
    carDatabase[1].upgrade.friction = 0.98f;
    carDatabase[1].upgrade.drag = 0.012f;
    carDatabase[1].upgrade.price = 2500.0f;
    carDatabase[1].upgrade.maxFuel = 55.0f;
    carDatabase[1].upgrade.fuelConsumption = 0.035f; 
    carDatabase[1].upgrade.insulation = 0.9f;
    carDatabase[1].upgrade.loadSensitivity = 0.8f;

    // 2. SUV (Heavy, Stable)
    strcpy(carDatabase[2].base.name, "SUV");
    strcpy(carDatabase[2].base.modelFileName, "suv.obj");
    carDatabase[2].base.maxSpeed = 70.0f; 
    carDatabase[2].base.acceleration = 0.6f;
    carDatabase[2].base.brakePower = 2.2f;
    carDatabase[2].base.turnSpeed = 2.6f;
    carDatabase[2].base.friction = 0.97f;
    carDatabase[2].base.drag = 0.025f;
    carDatabase[2].base.price = 3000.0f;
    carDatabase[2].base.maxFuel = 70.0f;
    carDatabase[2].base.fuelConsumption = 0.05f; 
    carDatabase[2].base.insulation = 0.5f;     
    carDatabase[2].base.loadSensitivity = 0.5f; 
    carDatabase[2].hasUpgrade = true;

    strcpy(carDatabase[2].upgrade.name, "SUV Luxury");
    strcpy(carDatabase[2].upgrade.modelFileName, "suv-luxury.obj");
    carDatabase[2].upgrade.maxSpeed = 85.0f;
    carDatabase[2].upgrade.acceleration = 0.5f; // Slower accel, higher top speed
    carDatabase[2].upgrade.brakePower = 2.8f;
    carDatabase[2].upgrade.turnSpeed = 2.7f;
    carDatabase[2].upgrade.friction = 0.97f;
    carDatabase[2].upgrade.drag = 0.022f;
    carDatabase[2].upgrade.price = 4500.0f;
    carDatabase[2].upgrade.maxFuel = 80.0f;
    carDatabase[2].upgrade.fuelConsumption = 0.06f; 
    carDatabase[2].upgrade.insulation = 0.4f;
    carDatabase[2].upgrade.loadSensitivity = 0.4f;

    // 3. Hatchback (Zippy)
    strcpy(carDatabase[3].base.name, "Hatchback Sport");
    strcpy(carDatabase[3].base.modelFileName, "hatchback-sports.obj");
    carDatabase[3].base.maxSpeed = 95.0f;
    carDatabase[3].base.acceleration = 0.65f;
    carDatabase[3].base.brakePower = 2.6f;
    carDatabase[3].base.turnSpeed = 3.5f;   // Very agile
    carDatabase[3].base.friction = 0.99f;   // Grippy
    carDatabase[3].base.drag = 0.018f;
    carDatabase[3].base.price = 2000.0f;
    carDatabase[3].base.maxFuel = 45.0f;
    carDatabase[3].base.fuelConsumption = 0.03f;
    carDatabase[3].base.insulation = 1.0f;     
    carDatabase[3].base.loadSensitivity = 0.9f;
    carDatabase[3].hasUpgrade = false;

    // 4. Race Car (Extreme)
    strcpy(carDatabase[4].base.name, "Race Car");
    strcpy(carDatabase[4].base.modelFileName, "race.obj");
    carDatabase[4].base.maxSpeed = 140.0f;
    carDatabase[4].base.acceleration = 0.9f;
    carDatabase[4].base.brakePower = 4.0f;
    carDatabase[4].base.turnSpeed = 3.5f;
    carDatabase[4].base.friction = 0.995f;
    carDatabase[4].base.drag = 0.01f;
    carDatabase[4].base.price = 10000.0f;
    carDatabase[4].base.maxFuel = 40.0f;       
    carDatabase[4].base.fuelConsumption = 0.08f; 
    carDatabase[4].base.insulation = 1.2f;     
    carDatabase[4].base.loadSensitivity = 1.5f; 
    carDatabase[4].hasUpgrade = true;

    strcpy(carDatabase[4].upgrade.name, "Race Future");
    strcpy(carDatabase[4].upgrade.modelFileName, "race-future.obj");
    carDatabase[4].upgrade.maxSpeed = 180.0f;
    carDatabase[4].upgrade.acceleration = 1.2f;
    carDatabase[4].upgrade.brakePower = 5.0f;
    carDatabase[4].upgrade.turnSpeed = 4.0f;
    carDatabase[4].upgrade.friction = 0.999f; // Glued to road
    carDatabase[4].upgrade.drag = 0.005f;
    carDatabase[4].upgrade.price = 25000.0f;
    carDatabase[4].upgrade.maxFuel = 100.0f;
    carDatabase[4].upgrade.fuelConsumption = 0.01f; 
    carDatabase[4].upgrade.insulation = 1.0f;
    carDatabase[4].upgrade.loadSensitivity = 1.2f;

    // 5. Heavy Truck (Slow, Drifty)
    strcpy(carDatabase[5].base.name, "Heavy Truck");
    strcpy(carDatabase[5].base.modelFileName, "truck.obj");
    carDatabase[5].base.maxSpeed = 55.0f;
    carDatabase[5].base.acceleration = 0.2f;
    carDatabase[5].base.brakePower = 1.5f;
    carDatabase[5].base.turnSpeed = 1.8f;   // Very slow turning
    carDatabase[5].base.friction = 0.95f;   // Slides more
    carDatabase[5].base.drag = 0.04f;       // High drag
    carDatabase[5].base.price = 4000.0f;
    carDatabase[5].base.maxFuel = 120.0f;
    carDatabase[5].base.fuelConsumption = 0.06f; 
    carDatabase[5].base.insulation = 0.6f;     
    carDatabase[5].base.loadSensitivity = 0.05f; // Ignores weight
    carDatabase[5].hasUpgrade = false;

    // 6. Family Van
    strcpy(carDatabase[6].base.name, "Family Van");
    strcpy(carDatabase[6].base.modelFileName, "van.obj");
    carDatabase[6].base.maxSpeed = 65.0f;
    carDatabase[6].base.acceleration = 0.35f;
    carDatabase[6].base.brakePower = 1.8f;
    carDatabase[6].base.turnSpeed = 2.4f;
    carDatabase[6].base.friction = 0.98f;
    carDatabase[6].base.drag = 0.025f;
    carDatabase[6].base.price = 1200.0f;
    carDatabase[6].base.maxFuel = 65.0f;
    carDatabase[6].base.fuelConsumption = 0.03f;
    carDatabase[6].base.insulation = 0.8f;
    carDatabase[6].base.loadSensitivity = 0.6f;
    carDatabase[6].hasUpgrade = false;
}

// --- Lifecycle Functions ---

void InitDealership() {
    InitCarDatabase();
    
    // Core Models
    tableModel = LoadModel("resources/Dealership/table-large.obj");
    screenModel = LoadModel("resources/Dealership/computer-screen.obj");
    wallModel = LoadModel("resources/Dealership/display-wall-wide.obj");
    systemModel = LoadModel("resources/Dealership/computer-system.obj");
    
    // New Props
    coneModel = LoadModel("resources/Props/cone.obj");
    railModel = LoadModel("resources/Dealership/rail.obj");
    containerModel = LoadModel("resources/Dealership/container.obj");
    containerOpenModel = LoadModel("resources/Dealership/container-flat-open.obj");
    skipModel = LoadModel("resources/Dealership/skip.obj");
    floorPanelModel = LoadModel("resources/Dealership/structure-panel.obj");
    
    // Setup Camera 
    shopCamera.position = (Vector3){ 12.0f, 7.0f, 12.0f };
    shopCamera.target = (Vector3){ 0.0f, 2.5f, 0.0f }; 
    shopCamera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    shopCamera.fovy = 50.0f; 
    shopCamera.projection = CAMERA_PERSPECTIVE;
}

void UnloadDealershipSystem() {
    UnloadModel(tableModel);
    UnloadModel(screenModel);
    UnloadModel(wallModel);
    UnloadModel(systemModel);
    UnloadModel(coneModel);
    UnloadModel(railModel);
    UnloadModel(containerModel);
    UnloadModel(containerOpenModel);
    UnloadModel(skipModel);
    UnloadModel(floorPanelModel);
}

DealershipState GetDealershipState() {
    return currentState;
}

void EnterDealership(Player *player) {
    currentState = DEALERSHIP_ACTIVE;
    currentSelection = 0;
    viewingUpgrade = false;
    carRotation = 0.0f;

    char pathBuffer[128];
    for (int i = 0; i < MAX_CARS; i++) {
        if (strlen(carDatabase[i].base.modelFileName) == 0) continue;

        sprintf(pathBuffer, "resources/Playermodels/%s", carDatabase[i].base.modelFileName);
        carDatabase[i].modelBase = LoadModel(pathBuffer);
        
        if (carDatabase[i].hasUpgrade) {
            sprintf(pathBuffer, "resources/Playermodels/%s", carDatabase[i].upgrade.modelFileName);
            carDatabase[i].modelUpgrade = LoadModel(pathBuffer);
        }
    }
}

void ExitDealership() {
    currentState = DEALERSHIP_INACTIVE;
    for (int i = 0; i < MAX_CARS; i++) {
        UnloadModel(carDatabase[i].modelBase);
        if (carDatabase[i].hasUpgrade) {
            UnloadModel(carDatabase[i].modelUpgrade);
        }
    }
}

// --- Logic & Rendering ---

void ApplyCarToPlayer(Player *player, CarStats stats, Model sourceModel) {
    AddMoney(player, "Vehicle Purchase", -stats.price);
    UnloadModel(player->model);

    char pathBuffer[128];
    sprintf(pathBuffer, "resources/Playermodels/%s", stats.modelFileName);
    player->model = LoadModel(pathBuffer);
    
    strcpy(player->currentModelFileName, stats.modelFileName);

    // [FIX] Sync all stats including turn speed and friction
    player->max_speed = stats.maxSpeed;
    player->acceleration = stats.acceleration;
    player->brake_power = stats.brakePower;
    player->turn_speed = stats.turnSpeed; // [NEW]
    player->friction = stats.friction;    // [NEW]
    player->drag = stats.drag;            // [NEW]
    
    player->maxFuel = stats.maxFuel;
    player->fuel = stats.maxFuel;
    player->fuelConsumption = stats.fuelConsumption;
    player->insulationFactor = stats.insulation;
    player->loadResistance = stats.loadSensitivity;
    
    BoundingBox box = GetModelBoundingBox(player->model);
    player->radius = (box.max.x - box.min.x) * 0.4f; 
}
// [UPDATED] Helper to switch cars
void SelectCar(Player *player, CarStats stats, Model sourceModel, int index, bool isUpgrade) {
    if (player->model.meshCount > 0) UnloadModel(player->model);

    char pathBuffer[128];
    sprintf(pathBuffer, "resources/Playermodels/%s", stats.modelFileName);
    player->model = LoadModel(pathBuffer);
    strcpy(player->currentModelFileName, stats.modelFileName);

    // Apply Physics
    player->max_speed = stats.maxSpeed;
    player->acceleration = stats.acceleration;
    player->brake_power = stats.brakePower;
    player->turn_speed = stats.turnSpeed;
    player->friction = stats.friction;
    player->drag = stats.drag;
    player->maxFuel = stats.maxFuel;
    player->fuelConsumption = stats.fuelConsumption;
    player->insulationFactor = stats.insulation;
    player->loadResistance = stats.loadSensitivity;
    
    if (player->fuel > player->maxFuel) player->fuel = player->maxFuel;
    BoundingBox box = GetModelBoundingBox(player->model);
    player->radius = (box.max.x - box.min.x) * 0.4f; 
    
    // [FIX] Update State including Upgrade Status
    player->currentCarIndex = index;
    player->isDrivingUpgrade = isUpgrade; 
    
    TraceLog(LOG_INFO, "DEALERSHIP: Switched to car %d (Upgrade: %d)", index, isUpgrade);
}

// [UPDATED] Buy function
void BuyCar(Player *player, CarStats stats, Model sourceModel, int index, bool isUpgrade) {
    AddMoney(player, "Vehicle Purchase", -stats.price);
    
    // [FIX] Set specific ownership flag
    if (isUpgrade) {
        player->ownedUpgrades[index] = true;
    } else {
        player->ownedCars[index] = true;
    }
    
    SelectCar(player, stats, sourceModel, index, isUpgrade);
}

void UpdateDealership(Player *player) {
    carRotation += 0.4f;

    if (IsKeyPressed(KEY_RIGHT)) {
        currentSelection++;
        if (currentSelection >= MAX_CARS) currentSelection = 0;
        viewingUpgrade = false; 
    }
    if (IsKeyPressed(KEY_LEFT)) {
        currentSelection--;
        if (currentSelection < 0) currentSelection = MAX_CARS - 1;
        viewingUpgrade = false;
    }

    if (carDatabase[currentSelection].hasUpgrade) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
            viewingUpgrade = !viewingUpgrade;
        }
    } else {
        viewingUpgrade = false;
    }

    CarStats *currentStats = viewingUpgrade ? &carDatabase[currentSelection].upgrade : &carDatabase[currentSelection].base;
    
    // [FIX] Ownership Logic
    bool isOwned = viewingUpgrade ? player->ownedUpgrades[currentSelection] : player->ownedCars[currentSelection];
    bool isCurrent = (player->currentCarIndex == currentSelection && player->isDrivingUpgrade == viewingUpgrade);

    if (IsKeyPressed(KEY_ENTER)) {
        Model targetModel = viewingUpgrade ? carDatabase[currentSelection].modelUpgrade : carDatabase[currentSelection].modelBase;
        
        if (isOwned) {
            if (!isCurrent) {
                SelectCar(player, *currentStats, targetModel, currentSelection, viewingUpgrade);
            }
        } 
        else if (player->money >= currentStats->price) {
            BuyCar(player, *currentStats, targetModel, currentSelection, viewingUpgrade);
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        ExitDealership();
    }
}

void DrawDealership(Player *player) {
    BeginMode3D(shopCamera);

        // 1. FLOOR (Checkerboard)
        Color floorA = (Color){ 30, 30, 35, 255 }; 
        Color floorB = (Color){ 40, 40, 45, 255 }; 
        for (int x = -20; x < 20; x+=2) {
            for (int z = -20; z < 20; z+=2) {
                Color c = ((x/2 + z/2) % 2 == 0) ? floorA : floorB;
                DrawCube((Vector3){(float)x, -0.1f, (float)z}, 2.0f, 0.1f, 2.0f, c);
            }
        }

        // 2. WALLS & WINDOWS
        Color wallColor = (Color){ 60, 60, 65, 255 };
        
        // Back Wall
        DrawCube((Vector3){0, 10, -15}, 60.0f, 30.0f, 1.0f, wallColor);
        DrawCube((Vector3){0, 5, -14.9f}, 60.0f, 0.5f, 1.0f, GOLD);
        DrawCube((Vector3){0, 6, -14.9f}, 60.0f, 0.5f, 1.0f, GOLD);
        // Fake Window (Back)
        DrawCube((Vector3){0, 12, -14.8f}, 20.0f, 8.0f, 0.1f, SKYBLUE);
        DrawCube((Vector3){0, 12, -14.7f}, 20.0f, 0.5f, 0.2f, DARKGRAY);
        DrawCube((Vector3){0, 12, -14.7f}, 0.5f, 8.0f, 0.2f, DARKGRAY);

        // Side Wall
        DrawCube((Vector3){-15, 10, 0}, 1.0f, 30.0f, 60.0f, wallColor);
        DrawCube((Vector3){-14.9f, 5, 0}, 1.0f, 0.5f, 60.0f, GOLD);
        DrawCube((Vector3){-14.9f, 6, 0}, 1.0f, 0.5f, 60.0f, GOLD);
        // Fake Window (Left)
        DrawCube((Vector3){-14.8f, 12, 0}, 0.1f, 8.0f, 20.0f, SKYBLUE); 
        DrawCube((Vector3){-14.7f, 12, 0}, 0.2f, 0.5f, 20.0f, DARKGRAY);
        DrawCube((Vector3){-14.7f, 12, 0}, 0.2f, 8.0f, 0.5f, DARKGRAY);

        // 3. STRUCTURE PANEL (Base)
        DrawModelEx(floorPanelModel, (Vector3){0, -0.05f, 0}, (Vector3){0,0,0}, 0.0f, (Vector3){25.0f, 1.0f, 25.0f}, GRAY);

        // 4. TABLE & FURNITURE (Massive Table)
        DrawModelEx(tableModel, (Vector3){0,0,0}, (Vector3){0,1,0}, 0.0f, (Vector3){4.5f, 4.5f, 4.5f}, WHITE);
        
        // --- OFFICE EQUIPMENT (Scaled to 3.0x) ---
        float officeScale = 3.0f;
        
        // BACK WALL SCREENS (Expanded)
        DrawModelEx(wallModel, (Vector3){0, 0, -9}, (Vector3){0,1,0}, 0.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(wallModel, (Vector3){-10, 0, -9}, (Vector3){0,1,0}, 15.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(wallModel, (Vector3){10, 0, -9}, (Vector3){0,1,0}, -15.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);

        // RIGHT WALL EQUIPMENT (Facing Left/Center) - Rotated 90
        // Systems against wall
        DrawModelEx(systemModel, (Vector3){13, 0, -2}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(systemModel, (Vector3){13, 0, 4}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        // Screens
        DrawModelEx(screenModel, (Vector3){13, 3.5f, 1}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){13, 3.5f, 7}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);

        // LEFT WALL EQUIPMENT (Facing Right/Center) - Rotated -90 (270)
        // Systems against wall
        DrawModelEx(systemModel, (Vector3){-13, 0, -2}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(systemModel, (Vector3){-13, 0, 4}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        // Screens
        DrawModelEx(screenModel, (Vector3){-13, 3.5f, 1}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){-13, 3.5f, 7}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);


        // 5. FLOOR PROPS (Scaled to 3.6f)
        float propScale = 3.6f;

        // Rails
        DrawModelEx(railModel, (Vector3){-14, 0, 10}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(railModel, (Vector3){14, 0, 10}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(railModel, (Vector3){0, 0, 14}, (Vector3){0,1,0}, 90.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(railModel, (Vector3){-10, 0, 12}, (Vector3){0,1,0}, 45.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(railModel, (Vector3){10, 0, 12}, (Vector3){0,1,0}, -45.0f, (Vector3){propScale, propScale, propScale}, WHITE);

        // Cones (Scattered)
        DrawModelEx(coneModel, (Vector3){-9, 0, 11}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(coneModel, (Vector3){-8, 0, 13}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(coneModel, (Vector3){10, 0, 9}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        
        // Large Containers
        DrawModelEx(containerModel, (Vector3){-18, 0, -14}, (Vector3){0,1,0}, 15.0f, (Vector3){22.0f, 22.0f, 22.0f}, WHITE);
        DrawModelEx(containerOpenModel, (Vector3){18, 0, -8}, (Vector3){0,1,0}, -20.0f, (Vector3){20.0f, 20.0f, 20.0f}, WHITE);
        
        // Skips
        DrawModelEx(skipModel, (Vector3){14, 0, -14}, (Vector3){0,1,0}, 30.0f, (Vector3){propScale, propScale, propScale}, WHITE);
        DrawModelEx(skipModel, (Vector3){-14, 0, -8}, (Vector3){0,1,0}, 0.0f, (Vector3){propScale, propScale, propScale}, WHITE);

        // 6. THE CAR (Lowered and 2x Size)
        Model *modelToDraw;
        if (viewingUpgrade) {
            modelToDraw = &carDatabase[currentSelection].modelUpgrade;
        } else {
            modelToDraw = &carDatabase[currentSelection].modelBase;
        }

        Vector3 carPos = { 0.0f, 2.0f, 0.0f }; 
        DrawModelEx(*modelToDraw, carPos, (Vector3){0,1.5,0}, carRotation, (Vector3){2.0f, 2.0f, 2.0f}, WHITE);
        
    EndMode3D();

    // --- 2D UI OVERLAY ---
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float uiScale = (float)screenH / 900.0f; 
    if (uiScale < 0.6f) uiScale = 0.6f;

    CarStats *stats = viewingUpgrade ? &carDatabase[currentSelection].upgrade : &carDatabase[currentSelection].base;
    
    // [FIX] Updated UI Logic
    bool isOwned = viewingUpgrade ? player->ownedUpgrades[currentSelection] : player->ownedCars[currentSelection];
    bool isCurrent = (player->currentCarIndex == currentSelection && player->isDrivingUpgrade == viewingUpgrade);

    // Fonts
    int fontTitle = (int)(50 * uiScale);
    int fontHeader = (int)(28 * uiScale);
    int fontLabel = (int)(18 * uiScale);
    int fontValue = (int)(20 * uiScale);
    int lineHeight = (int)(25 * uiScale);

    // Header & Nav
    DrawText("DEALERSHIP", (int)(30*uiScale), (int)(30*uiScale), fontTitle, BLACK);
    DrawText("DEALERSHIP", (int)(28*uiScale), (int)(28*uiScale), fontTitle, GOLD);
    DrawText("Backspace to Exit", (int)(30*uiScale), (int)(80*uiScale), fontLabel, LIGHTGRAY);
    DrawText("<", screenW * 0.15f, screenH/2, (int)(60*uiScale), WHITE);
    DrawText(">", screenW * 0.85f, screenH/2, (int)(60*uiScale), WHITE);

    // --- STATS PANEL ---
    int panelW = (int)(screenW * 0.28f); 
    if (panelW < 380) panelW = 380;
    int panelH = (int)(screenH * 0.85f);
    int panelX = screenW - panelW - (int)(30*uiScale);
    int panelY = (screenH - panelH) / 2;
    
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.92f));
    DrawRectangleLines(panelX, panelY, panelW, panelH, DARKGRAY);
    
    int tx = panelX + (int)(25*uiScale);
    int ty = panelY + (int)(30*uiScale);

    // 1. CAR IDENTITY
    DrawText(stats->name, tx, ty, fontHeader, WHITE);
    ty += (int)(45 * uiScale);
    DrawLine(tx, ty, tx + panelW - 50, ty, GRAY);
    ty += (int)(20 * uiScale);

    // 2. PERFORMANCE COLUMN
    DrawText("PERFORMANCE", tx, ty, fontLabel, GOLD);
    ty += (int)(30 * uiScale);
    int col1 = tx;
    int col2 = tx + (int)(160 * uiScale);

    DrawText("Top Speed", col1, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.0f km/h", stats->maxSpeed), col2, ty, fontValue, WHITE);
    ty += lineHeight;

    DrawText("Acceleration", col1, ty, fontLabel, LIGHTGRAY);
    DrawRectangle(col2, ty + 5, (int)(stats->acceleration * 100 * uiScale), 10, GREEN);
    ty += lineHeight;

    DrawText("Braking", col1, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.1f", stats->brakePower), col2, ty, fontValue, WHITE);
    ty += lineHeight;

    DrawText("Handling", col1, ty, fontLabel, LIGHTGRAY);
    const char* handStr = "Normal";
    if (stats->turnSpeed > 3.0f) handStr = "Sport";
    if (stats->turnSpeed < 2.0f) handStr = "Heavy";
    DrawText(handStr, col2, ty, fontValue, WHITE);
    ty += lineHeight * 2;

    // 3. UTILITY COLUMN
    DrawText("UTILITY & EFFICIENCY", tx, ty, fontLabel, SKYBLUE);
    ty += (int)(30 * uiScale);

    DrawText("Fuel Tank", col1, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.0f L", stats->maxFuel), col2, ty, fontValue, WHITE);
    ty += lineHeight;

    DrawText("Efficiency", col1, ty, fontLabel, LIGHTGRAY);
    const char* ecoStr = "Avg";
    if (stats->fuelConsumption < 0.03f) ecoStr = "Good";
    if (stats->fuelConsumption > 0.05f) ecoStr = "Poor";
    DrawText(ecoStr, col2, ty, fontValue, WHITE);
    ty += lineHeight;

    DrawText("Insulation", col1, ty, fontLabel, LIGHTGRAY);
    const char* insStr = "Standard";
    Color insCol = WHITE;
    if (stats->insulation < 0.1f) { insStr = "Perfect"; insCol = GREEN; }
    else if (stats->insulation > 1.0f) { insStr = "Open Air"; insCol = ORANGE; }
    DrawText(insStr, col2, ty, fontValue, insCol);
    ty += lineHeight;

    DrawText("Suspension", col1, ty, fontLabel, LIGHTGRAY);
    const char* susStr = "Normal";
    Color susCol = WHITE;
    if (stats->loadSensitivity < 0.2f) { susStr = "Heavy Duty"; susCol = GREEN; }
    else if (stats->loadSensitivity > 1.2f) { susStr = "Stiff"; susCol = RED; }
    DrawText(susStr, col2, ty, fontValue, susCol);
    
    // --- BOTTOM SECTION: BUY/SELECT ---
    ty = panelY + panelH - (int)(140 * uiScale);

    if (isOwned) {
        if (isCurrent) {
            DrawRectangle(panelX + 20, ty, panelW - 40, 50, GRAY);
            DrawText("CURRENTLY DRIVING", panelX + 45, ty + 15, fontLabel, BLACK);
        } else {
            DrawRectangle(panelX + 20, ty, panelW - 40, 50, SKYBLUE);
            DrawText("PRESS ENTER TO DRIVE", panelX + 35, ty + 15, fontLabel, BLACK);
        }
    } else {
        DrawText(TextFormat("Price: $%.0f", stats->price), tx, ty - 40, fontHeader, GREEN);
        
        if (player->money >= stats->price) {
            DrawRectangle(panelX + 20, ty, panelW - 40, 50, GREEN);
            DrawText("PRESS ENTER TO BUY", panelX + 40, ty + 15, fontLabel, BLACK);
        } else {
            DrawRectangle(panelX + 20, ty, panelW - 40, 50, RED);
            DrawText("INSUFFICIENT FUNDS", panelX + 40, ty + 15, fontLabel, WHITE);
        }
    }

    // --- UPGRADE HINT ---
    if (carDatabase[currentSelection].hasUpgrade) {
        int cx = screenW / 2;
        int cy = screenH - (int)(100*uiScale);
        
        if (viewingUpgrade) {
             const char* txt = "Viewing: SPORT / LUXURY Trim";
             DrawText(txt, cx - MeasureText(txt, fontValue)/2, cy, fontValue, GOLD);
             const char* sub = "(Press UP/DOWN for Base Model)";
             DrawText(sub, cx - MeasureText(sub, fontLabel)/2, cy + 30, fontLabel, LIGHTGRAY);
        } else {
             const char* txt = "Viewing: BASE Model";
             DrawText(txt, cx - MeasureText(txt, fontValue)/2, cy, fontValue, WHITE);
             const char* sub = "(Press UP/DOWN for Upgraded Trim)";
             DrawText(sub, cx - MeasureText(sub, fontLabel)/2, cy + 30, fontLabel, GOLD);
        }
    }
}
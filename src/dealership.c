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

#include "dealership.h"
#include "raymath.h"
#include "player.h" 
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
    
    // Physics Handling
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

/*
 * Description: Populates the car database with hardcoded stats for all vehicle types.
 * Parameters: None.
 * Returns: None.
 */
void InitCarDatabase() {
    // 0. Delivery Van (Balanced)
    strcpy(carDatabase[0].base.name, "Delivery Van");
    strcpy(carDatabase[0].base.modelFileName, "delivery.obj");
    carDatabase[0].base.maxSpeed = 15.0f;        // Approx 80 km/h visual feel
    carDatabase[0].base.acceleration = 0.7f;     // Good pickup
    carDatabase[0].base.brakePower = 3.0f;       // Solid brakes
    carDatabase[0].base.turnSpeed = 1.6f;
    carDatabase[0].base.friction = 0.992f;       // Standard rolling resistance
    carDatabase[0].base.drag = 0.002f;
    carDatabase[0].base.price = 5000.0f;
    carDatabase[0].base.maxFuel = 80.0f;
    carDatabase[0].base.fuelConsumption = 0.04f;
    carDatabase[0].base.insulation = 0.7f;
    carDatabase[0].base.loadSensitivity = 0.3f;
    carDatabase[0].hasUpgrade = false;

    // 1. Sedan (Agile - Default Player Car)
    strcpy(carDatabase[1].base.name, "Sedan Standard");
    strcpy(carDatabase[1].base.modelFileName, "sedan.obj");
    carDatabase[1].base.maxSpeed = 18.0f;
    carDatabase[1].base.acceleration = 1.0f;     // Snappy
    carDatabase[1].base.brakePower = 3.8f;
    carDatabase[1].base.turnSpeed = 2.0f;
    carDatabase[1].base.friction = 0.995f;       // Glides well
    carDatabase[1].base.drag = 0.0015f;
    carDatabase[1].base.price = 1500.0f;
    carDatabase[1].base.maxFuel = 50.0f;
    carDatabase[1].base.fuelConsumption = 0.02f;
    carDatabase[1].base.insulation = 0.9f;
    carDatabase[1].base.loadSensitivity = 0.8f;
    carDatabase[1].hasUpgrade = true;

    strcpy(carDatabase[1].upgrade.name, "Sedan Sport");
    strcpy(carDatabase[1].upgrade.modelFileName, "sedan-sports.obj");
    carDatabase[1].upgrade.maxSpeed = 22.0f;
    carDatabase[1].upgrade.acceleration = 1.2f;
    carDatabase[1].upgrade.brakePower = 4.2f;
    carDatabase[1].upgrade.turnSpeed = 2.7f;
    carDatabase[1].upgrade.friction = 0.996f;
    carDatabase[1].upgrade.drag = 0.001f;
    carDatabase[1].upgrade.price = 2500.0f;
    carDatabase[1].upgrade.maxFuel = 55.0f;
    carDatabase[1].upgrade.fuelConsumption = 0.035f;
    carDatabase[1].upgrade.insulation = 0.9f;
    carDatabase[1].upgrade.loadSensitivity = 0.8f;

    // 2. SUV (Heavy, Stable)
    strcpy(carDatabase[2].base.name, "SUV");
    strcpy(carDatabase[2].base.modelFileName, "suv.obj");
    carDatabase[2].base.maxSpeed = 20.0f;
    carDatabase[2].base.acceleration = 0.9f;
    carDatabase[2].base.brakePower = 3.7f;
    carDatabase[2].base.turnSpeed = 1.6f;
    carDatabase[2].base.friction = 0.990f;       // Heavier, rolls less
    carDatabase[2].base.drag = 0.003f;
    carDatabase[2].base.price = 3000.0f;
    carDatabase[2].base.maxFuel = 70.0f;
    carDatabase[2].base.fuelConsumption = 0.05f;
    carDatabase[2].base.insulation = 0.5f;
    carDatabase[2].base.loadSensitivity = 0.5f;
    carDatabase[2].hasUpgrade = true;

    strcpy(carDatabase[2].upgrade.name, "SUV Luxury");
    strcpy(carDatabase[2].upgrade.modelFileName, "suv-luxury.obj");
    carDatabase[2].upgrade.maxSpeed = 25.0f;
    carDatabase[2].upgrade.acceleration = 0.9f;
    carDatabase[2].upgrade.brakePower = 3.8f;
    carDatabase[2].upgrade.turnSpeed = 1.8f;
    carDatabase[2].upgrade.friction = 0.992f;
    carDatabase[2].upgrade.drag = 0.0025f;
    carDatabase[2].upgrade.price = 4500.0f;
    carDatabase[2].upgrade.maxFuel = 80.0f;
    carDatabase[2].upgrade.fuelConsumption = 0.06f;
    carDatabase[2].upgrade.insulation = 0.4f;
    carDatabase[2].upgrade.loadSensitivity = 0.4f;

    // 3. Hatchback (Zippy)
    strcpy(carDatabase[3].base.name, "Hatchback Sport");
    strcpy(carDatabase[3].base.modelFileName, "hatchback-sports.obj");
    carDatabase[3].base.maxSpeed = 30.0f;
    carDatabase[3].base.acceleration = 1.1f;
    carDatabase[3].base.brakePower = 3.8f;
    carDatabase[3].base.turnSpeed = 2.8f;       // Very agile
    carDatabase[3].base.friction = 0.995f;
    carDatabase[3].base.drag = 0.002f;
    carDatabase[3].base.price = 2000.0f;
    carDatabase[3].base.maxFuel = 45.0f;
    carDatabase[3].base.fuelConsumption = 0.03f;
    carDatabase[3].base.insulation = 1.0f;
    carDatabase[3].base.loadSensitivity = 0.9f;
    carDatabase[3].hasUpgrade = false;

    // 4. Race Car (Extreme)
    strcpy(carDatabase[4].base.name, "Race Car");
    strcpy(carDatabase[4].base.modelFileName, "race.obj");
    carDatabase[4].base.maxSpeed = 40.0f;
    carDatabase[4].base.acceleration = 1.3f;
    carDatabase[4].base.brakePower = 5.0f;      // Strong brakes
    carDatabase[4].base.turnSpeed = 3.6f;
    carDatabase[4].base.friction = 0.998f;      // Very low rolling resistance
    carDatabase[4].base.drag = 0.001f;
    carDatabase[4].base.price = 10000.0f;
    carDatabase[4].base.maxFuel = 40.0f;
    carDatabase[4].base.fuelConsumption = 0.08f;
    carDatabase[4].base.insulation = 1.2f;
    carDatabase[4].base.loadSensitivity = 1.5f;
    carDatabase[4].hasUpgrade = true;

    strcpy(carDatabase[4].upgrade.name, "Race Future");
    strcpy(carDatabase[4].upgrade.modelFileName, "race-future.obj");
    carDatabase[4].upgrade.maxSpeed = 50.0f;
    carDatabase[4].upgrade.acceleration = 2.0f;
    carDatabase[4].upgrade.brakePower = 7.2f;
    carDatabase[4].upgrade.turnSpeed = 4.5f;
    carDatabase[4].upgrade.friction = 0.999f;
    carDatabase[4].upgrade.drag = 0.0005f;
    carDatabase[4].upgrade.price = 25000.0f;
    carDatabase[4].upgrade.maxFuel = 100.0f;
    carDatabase[4].upgrade.fuelConsumption = 0.01f;
    carDatabase[4].upgrade.insulation = 1.0f;
    carDatabase[4].upgrade.loadSensitivity = 1.2f;

    // 5. Heavy Truck (Slow)
    strcpy(carDatabase[5].base.name, "Heavy Truck");
    strcpy(carDatabase[5].base.modelFileName, "truck.obj");
    carDatabase[5].base.maxSpeed = 18.0f;
    carDatabase[5].base.acceleration = 0.9f;     // Slow start
    carDatabase[5].base.brakePower = 3.0f;       // Takes time to stop
    carDatabase[5].base.turnSpeed = 1.7f;
    carDatabase[5].base.friction = 0.989f;       // High resistance
    carDatabase[5].base.drag = 0.005f;
    carDatabase[5].base.price = 4000.0f;
    carDatabase[5].base.maxFuel = 120.0f;
    carDatabase[5].base.fuelConsumption = 0.06f;
    carDatabase[5].base.insulation = 0.6f;
    carDatabase[5].base.loadSensitivity = 0.05f;
    carDatabase[5].hasUpgrade = false;

    // 6. Family Van
    strcpy(carDatabase[6].base.name, "Family Van");
    strcpy(carDatabase[6].base.modelFileName, "van.obj");
    carDatabase[6].base.maxSpeed = 20.0f;
    carDatabase[6].base.acceleration = 1.0f;
    carDatabase[6].base.brakePower = 2.8f;
    carDatabase[6].base.turnSpeed = 1.5f;
    carDatabase[6].base.friction = 0.992f;
    carDatabase[6].base.drag = 0.003f;
    carDatabase[6].base.price = 1200.0f;
    carDatabase[6].base.maxFuel = 65.0f;
    carDatabase[6].base.fuelConsumption = 0.03f;
    carDatabase[6].base.insulation = 0.8f;
    carDatabase[6].base.loadSensitivity = 0.6f;
    carDatabase[6].hasUpgrade = false;
}

// --- Lifecycle Functions ---

/*
 * Description: Initializes the dealership system, including 3D models and the camera.
 * Parameters: None.
 * Returns: None.
 */
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

/*
 * Description: Unloads all models associated with the dealership to free memory.
 * Parameters: None.
 * Returns: None.
 */
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

/*
 * Description: Returns the current state of the dealership interaction.
 * Parameters: None.
 * Returns: The current DealershipState enum value.
 */
DealershipState GetDealershipState() {
    return currentState;
}

/*
 * Description: Transitions the game into the dealership mode and loads vehicle models for display.
 * Parameters:
 * - player: Pointer to the player (unused in function body but kept for signature consistency).
 * Returns: None.
 */
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

/*
 * Description: Exits the dealership mode and unloads vehicle display models.
 * Parameters: None.
 * Returns: None.
 */
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

/*
 * Description: Applies vehicle statistics to the player struct and loads the new model.
 * Parameters:
 * - player: Pointer to the Player to update.
 * - stats: CarStats struct containing the new vehicle data.
 * - sourceModel: The 3D model of the car (unused, reloaded from file).
 * Returns: None.
 */
void ApplyCarToPlayer(Player *player, CarStats stats, Model sourceModel) {
    AddMoney(player, "Vehicle Purchase", -stats.price);
    UnloadModel(player->model);

    char pathBuffer[128];
    sprintf(pathBuffer, "resources/Playermodels/%s", stats.modelFileName);
    player->model = LoadModel(pathBuffer);
    
    strcpy(player->currentModelFileName, stats.modelFileName);

    // Sync all stats
    player->max_speed = stats.maxSpeed;
    player->acceleration = stats.acceleration;
    player->brake_power = stats.brakePower;
    player->turn_speed = stats.turnSpeed; 
    player->friction = stats.friction;    
    player->drag = stats.drag;            
    
    player->maxFuel = stats.maxFuel;
    player->fuel = stats.maxFuel;
    player->fuelConsumption = stats.fuelConsumption;
    player->insulationFactor = stats.insulation;
    player->loadResistance = stats.loadSensitivity;
    
    BoundingBox box = GetModelBoundingBox(player->model);
    player->radius = (box.max.x - box.min.x) * 0.4f; 
}

/*
 * Description: Selects a specific car for the player, updating models and physics stats.
 * Parameters:
 * - player: Pointer to the Player.
 * - stats: Stats of the car to select.
 * - sourceModel: Model reference (unused).
 * - index: Index of the car in the database.
 * - isUpgrade: Boolean flag indicating if it's the upgraded version.
 * Returns: None.
 */
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
    
    // Update State including Upgrade Status
    player->currentCarIndex = index;
    player->isDrivingUpgrade = isUpgrade; 
    
    TraceLog(LOG_INFO, "DEALERSHIP: Switched to car %d (Upgrade: %d)", index, isUpgrade);
}

/*
 * Description: Processes a vehicle purchase, updates money, sets ownership, and selects the car.
 * Parameters:
 * - player: Pointer to the Player.
 * - stats: Stats of the car to buy.
 * - sourceModel: Model reference.
 * - index: Database index.
 * - isUpgrade: Boolean flag for upgraded version.
 * Returns: None.
 */
void BuyCar(Player *player, CarStats stats, Model sourceModel, int index, bool isUpgrade) {
    AddMoney(player, "Vehicle Purchase", -stats.price);
    
    if (isUpgrade) {
        player->ownedUpgrades[index] = true;
    } else {
        player->ownedCars[index] = true;
    }
    
    SelectCar(player, stats, sourceModel, index, isUpgrade);
}

/*
 * Description: Handles user input within the dealership (navigation, buying, selecting).
 * Parameters:
 * - player: Pointer to the Player.
 * Returns: None.
 */
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

/*
 * Description: Renders the 3D dealership scene and the 2D UI overlay.
 * Parameters:
 * - player: Pointer to the Player.
 * Returns: None.
 */
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
        
        // --- OFFICE EQUIPMENT ---
        float officeScale = 3.0f;
        
        // BACK WALL SCREENS (Expanded)
        DrawModelEx(wallModel, (Vector3){0, 0, -9}, (Vector3){0,1,0}, 0.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(wallModel, (Vector3){-10, 0, -9}, (Vector3){0,1,0}, 15.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(wallModel, (Vector3){10, 0, -9}, (Vector3){0,1,0}, -15.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);

        // RIGHT WALL EQUIPMENT (Facing Left/Center) - Rotated 90
        DrawModelEx(systemModel, (Vector3){13, 0, -2}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(systemModel, (Vector3){13, 0, 4}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){13, 3.5f, 1}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){13, 3.5f, 7}, (Vector3){0,1,0}, -90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);

        // LEFT WALL EQUIPMENT (Facing Right/Center) - Rotated -90 (270)
        DrawModelEx(systemModel, (Vector3){-13, 0, -2}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(systemModel, (Vector3){-13, 0, 4}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){-13, 3.5f, 1}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);
        DrawModelEx(screenModel, (Vector3){-13, 3.5f, 7}, (Vector3){0,1,0}, 90.0f, (Vector3){officeScale, officeScale, officeScale}, WHITE);

        // 5. FLOOR PROPS
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
    
    // UI Logic
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
    DrawText(TextFormat("%.0f km/h", stats->maxSpeed*4.0f), col2, ty, fontValue, WHITE);
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
    ty = panelY + panelH - (int)(180 * uiScale); 
    
    int boxH = 76;
    int textY = ty + (boxH / 2) - (fontLabel / 2); // Perfectly centered text

    if (isOwned) {
        if (isCurrent) {
            DrawRectangle(panelX + 20, ty, panelW - 40, boxH, GRAY);
            DrawText("CURRENTLY DRIVING", panelX + 45, textY, fontLabel, BLACK);
        } else {
            DrawRectangle(panelX + 20, ty, panelW - 40, boxH, SKYBLUE);
            DrawText("PRESS ENTER TO DRIVE", panelX + 35, textY, fontLabel, BLACK);
        }
    } else {
        DrawText(TextFormat("Price: $%.0f", stats->price), tx, ty - 60, fontHeader, GREEN);
        
        if (player->money >= stats->price) {
            DrawRectangle(panelX + 20, ty, panelW - 40, boxH, GREEN);
            DrawText("PRESS ENTER TO BUY", panelX + 40, textY, fontLabel, BLACK);
        } else {
            DrawRectangle(panelX + 20, ty, panelW - 40, boxH, RED);
            DrawText("INSUFFICIENT FUNDS", panelX + 40, textY, fontLabel, WHITE);
        }
    }

    // --- PLAYER MONEY DISPLAY ---
    const char* moneyText = TextFormat("Balance: $%.2f", player->money);
    int mSize = (int)(30 * uiScale);
    int mWidth = MeasureText(moneyText, mSize);
    int mPad = 10;
    
    // Position: Top Right
    int mX = screenW - mWidth - (mPad * 3);
    int mY = mPad * 2;

    // Draw Dark Background Box
    DrawRectangle(mX - mPad, mY - mPad, mWidth + (mPad * 2), mSize + (mPad * 2), Fade(BLACK, 0.8f));
    
    // Draw Green Text
    DrawText(moneyText, mX, mY, mSize, GREEN);

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
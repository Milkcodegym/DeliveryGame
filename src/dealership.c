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

// Models
static Model tableModel;
static Model screenModel;
static Model wallModel;
static Model systemModel;
static Model coneModel;
// Removed: static Model trashModel; 
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

// --- Database Population (Unchanged) ---
void InitCarDatabase() {
    // 0. Delivery Van
    strcpy(carDatabase[0].base.name, "Delivery Van");
    strcpy(carDatabase[0].base.modelFileName, "delivery.obj");
    carDatabase[0].base.maxSpeed = 60.0f; 
    carDatabase[0].base.acceleration = 0.3f;
    carDatabase[0].base.brakePower = 2.0f;
    carDatabase[0].base.price = 500.0f;
    carDatabase[0].base.maxFuel = 80.0f;       
    carDatabase[0].base.fuelConsumption = 0.04f; 
    carDatabase[0].base.insulation = 0.7f;     
    carDatabase[0].base.loadSensitivity = 0.3f; 
    carDatabase[0].hasUpgrade = false;

    // 1. Sedan
    strcpy(carDatabase[1].base.name, "Sedan Standard");
    strcpy(carDatabase[1].base.modelFileName, "sedan.obj");
    carDatabase[1].base.maxSpeed = 80.0f; 
    carDatabase[1].base.acceleration = 0.5f;
    carDatabase[1].base.brakePower = 2.5f;
    carDatabase[1].base.price = 1500.0f;
    carDatabase[1].base.maxFuel = 50.0f;
    carDatabase[1].base.fuelConsumption = 0.02f; 
    carDatabase[1].base.insulation = 0.9f;     
    carDatabase[1].base.loadSensitivity = 0.8f; 
    carDatabase[1].hasUpgrade = true;

    strcpy(carDatabase[1].upgrade.name, "Sedan Sport");
    strcpy(carDatabase[1].upgrade.modelFileName, "sedan-sport.obj");
    carDatabase[1].upgrade.maxSpeed = 110.0f;
    carDatabase[1].upgrade.acceleration = 0.7f;
    carDatabase[1].upgrade.brakePower = 3.0f;
    carDatabase[1].upgrade.price = 2500.0f;
    carDatabase[1].upgrade.maxFuel = 55.0f;
    carDatabase[1].upgrade.fuelConsumption = 0.035f; 
    carDatabase[1].upgrade.insulation = 0.9f;
    carDatabase[1].upgrade.loadSensitivity = 0.8f;

    // 2. SUV
    strcpy(carDatabase[2].base.name, "SUV");
    strcpy(carDatabase[2].base.modelFileName, "suv.obj");
    carDatabase[2].base.maxSpeed = 70.0f; 
    carDatabase[2].base.acceleration = 0.6f;
    carDatabase[2].base.brakePower = 2.2f;
    carDatabase[2].base.price = 3000.0f;
    carDatabase[2].base.maxFuel = 70.0f;
    carDatabase[2].base.fuelConsumption = 0.05f; 
    carDatabase[2].base.insulation = 0.5f;     
    carDatabase[2].base.loadSensitivity = 0.5f; 
    carDatabase[2].hasUpgrade = true;

    strcpy(carDatabase[2].upgrade.name, "SUV Luxury");
    strcpy(carDatabase[2].upgrade.modelFileName, "suv-luxury.obj");
    carDatabase[2].upgrade.maxSpeed = 85.0f;
    carDatabase[2].upgrade.acceleration = 0.5f;
    carDatabase[2].upgrade.brakePower = 2.8f;
    carDatabase[2].upgrade.price = 4500.0f;
    carDatabase[2].upgrade.maxFuel = 80.0f;
    carDatabase[2].upgrade.fuelConsumption = 0.06f; 
    carDatabase[2].upgrade.insulation = 0.4f;
    carDatabase[2].upgrade.loadSensitivity = 0.4f;

    // 3. Hatchback
    strcpy(carDatabase[3].base.name, "Hatchback Sport");
    strcpy(carDatabase[3].base.modelFileName, "hatchback-sport.obj");
    carDatabase[3].base.maxSpeed = 95.0f;
    carDatabase[3].base.acceleration = 0.65f;
    carDatabase[3].base.brakePower = 2.6f;
    carDatabase[3].base.price = 2000.0f;
    carDatabase[3].base.maxFuel = 45.0f;
    carDatabase[3].base.fuelConsumption = 0.03f;
    carDatabase[3].base.insulation = 1.0f;     
    carDatabase[3].base.loadSensitivity = 0.9f;
    carDatabase[3].hasUpgrade = false;

    // 4. Race Car
    strcpy(carDatabase[4].base.name, "Race Car");
    strcpy(carDatabase[4].base.modelFileName, "race.obj");
    carDatabase[4].base.maxSpeed = 140.0f;
    carDatabase[4].base.acceleration = 0.9f;
    carDatabase[4].base.brakePower = 4.0f;
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
    carDatabase[4].upgrade.price = 25000.0f;
    carDatabase[4].upgrade.maxFuel = 100.0f;
    carDatabase[4].upgrade.fuelConsumption = 0.01f; 
    carDatabase[4].upgrade.insulation = 1.0f;
    carDatabase[4].upgrade.loadSensitivity = 1.2f;

    // 5. Heavy Truck
    strcpy(carDatabase[5].base.name, "Heavy Truck");
    strcpy(carDatabase[5].base.modelFileName, "truck.obj");
    carDatabase[5].base.maxSpeed = 55.0f;
    carDatabase[5].base.acceleration = 0.2f;
    carDatabase[5].base.brakePower = 1.5f;
    carDatabase[5].base.price = 4000.0f;
    carDatabase[5].base.maxFuel = 120.0f;
    carDatabase[5].base.fuelConsumption = 0.06f; 
    carDatabase[5].base.insulation = 0.6f;     
    carDatabase[5].base.loadSensitivity = 0.05f; 
    carDatabase[5].hasUpgrade = false;

    // 6. Family Van
    strcpy(carDatabase[6].base.name, "Family Van");
    strcpy(carDatabase[6].base.modelFileName, "van.obj");
    carDatabase[6].base.maxSpeed = 65.0f;
    carDatabase[6].base.acceleration = 0.35f;
    carDatabase[6].base.brakePower = 1.8f;
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
    // Removed: trashModel = LoadModel("resources/random/trash.obj");
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
    // Removed: UnloadModel(trashModel);
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

    player->max_speed = stats.maxSpeed;
    player->acceleration = stats.acceleration;
    player->brake_power = stats.brakePower;
    player->maxFuel = stats.maxFuel;
    player->fuel = stats.maxFuel;
    player->fuelConsumption = stats.fuelConsumption;
    player->insulationFactor = stats.insulation;
    player->loadResistance = stats.loadSensitivity;
    
    BoundingBox box = GetModelBoundingBox(player->model);
    player->radius = (box.max.x - box.min.x) * 0.4f; 
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
    }

    CarStats *currentStats = viewingUpgrade ? &carDatabase[currentSelection].upgrade : &carDatabase[currentSelection].base;
    
    if (IsKeyPressed(KEY_ENTER)) {
        if (player->money >= currentStats->price) {
            Model targetModel = viewingUpgrade ? carDatabase[currentSelection].modelUpgrade : carDatabase[currentSelection].modelBase;
            ApplyCarToPlayer(player, *currentStats, targetModel);
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        ExitDealership();
    }
}

void DrawDealership(Player *player) {
    // --- 3D Scene ---
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

    // --- 2D UI Overlay (Dynamic Scaling) ---
    
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    
    float uiScale = (float)screenH / 1000.0f; 
    if (uiScale < 0.6f) uiScale = 0.6f;

    CarStats *stats = viewingUpgrade ? &carDatabase[currentSelection].upgrade : &carDatabase[currentSelection].base;

    // Fonts & Sizes
    int fontTitle = (int)(50 * uiScale);
    int fontHeader = (int)(30 * uiScale);
    int fontLabel = (int)(20 * uiScale);
    int fontValue = (int)(22 * uiScale);
    int gap = (int)(35 * uiScale);

    // Header
    DrawText("DEALERSHIP", (int)(30*uiScale), (int)(30*uiScale), fontTitle, BLACK);
    DrawText("DEALERSHIP", (int)(28*uiScale), (int)(28*uiScale), fontTitle, GOLD);
    DrawText("Backspace to Exit", (int)(30*uiScale), (int)(80*uiScale), fontLabel, LIGHTGRAY);

    // Navigation Arrows
    DrawText("<", screenW * 0.15f, screenH/2, (int)(60*uiScale), WHITE);
    DrawText(">", screenW * 0.85f, screenH/2, (int)(60*uiScale), WHITE);

    // --- Stats Panel (Right Side) ---
    int panelW = (int)(screenW * 0.25f);
    if (panelW < (int)(350 * uiScale)) panelW = (int)(350 * uiScale);
    
    int panelH = (int)(screenH * 0.8f);
    int panelX = screenW - panelW - (int)(40*uiScale);
    int panelY = (screenH - panelH) / 2;
    
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.9f));
    DrawRectangleLines(panelX, panelY, panelW, panelH, DARKGRAY);
    
    int tx = panelX + (int)(30*uiScale);
    int ty = panelY + (int)(40*uiScale);

    // Car Name
    DrawText(stats->name, tx, ty, fontHeader, WHITE);
    ty += (int)(50 * uiScale);
    
    DrawLine(tx, ty, tx + panelW - (int)(60*uiScale), ty, LIGHTGRAY);
    ty += (int)(30 * uiScale);

    // Price
    DrawText(TextFormat("Price: $%.0f", stats->price), tx, ty, fontHeader, GREEN);
    ty += (int)(50 * uiScale);

    // Stats
    DrawText("PERFORMANCE", tx, ty, (int)(18*uiScale), GOLD);
    ty += (int)(30 * uiScale);
    
    int valOff = (int)(140 * uiScale);

    DrawText("Top Speed", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.0f km/h", stats->maxSpeed), tx + valOff, ty, fontValue, WHITE);
    ty += gap;
    
    DrawText("Acceleration", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.1f", stats->acceleration), tx + valOff, ty, fontValue, WHITE);
    ty += gap;
    
    DrawText("Braking", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.1f", stats->brakePower), tx + valOff, ty, fontValue, WHITE);
    ty += gap + (int)(20*uiScale);

    DrawText("UTILITY", tx, ty, (int)(18*uiScale), SKYBLUE);
    ty += (int)(30 * uiScale);
    
    DrawText("Fuel Tank", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(TextFormat("%.0f L", stats->maxFuel), tx + valOff, ty, fontValue, WHITE);
    ty += gap;

    const char* insText = "Normal";
    Color insColor = WHITE;
    if (stats->insulation < 0.6f) { insText = "Excellent"; insColor = GREEN; }
    else if (stats->insulation > 1.0f) { insText = "Poor"; insColor = RED; }
    
    DrawText("Insulation", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(insText, tx + valOff, ty, fontValue, insColor);
    ty += gap;

    const char* loadText = "Standard";
    Color loadColor = WHITE;
    if (stats->loadSensitivity < 0.4f) { loadText = "Heavy Duty"; loadColor = GREEN; }
    else if (stats->loadSensitivity > 1.2f) { loadText = "Fragile"; loadColor = ORANGE; }

    DrawText("Cargo Cap", tx, ty, fontLabel, LIGHTGRAY);
    DrawText(loadText, tx + valOff, ty, fontValue, loadColor);
    ty += gap + (int)(40*uiScale);

    // Wallet & Buy
    DrawText(TextFormat("Your Wallet: $%.0f", player->money), tx, ty, fontLabel, LIGHTGRAY);
    ty += (int)(40*uiScale);

    Rectangle buyRect = { panelX + 20*uiScale, ty, panelW - 40*uiScale, 60*uiScale };
    if (player->money >= stats->price) {
        DrawRectangleRec(buyRect, GREEN);
        DrawText("PRESS ENTER TO BUY", buyRect.x + 20*uiScale, buyRect.y + 20*uiScale, fontLabel, BLACK);
    } else {
        DrawRectangleRec(buyRect, DARKGRAY);
        DrawText("INSUFFICIENT FUNDS", buyRect.x + 20*uiScale, buyRect.y + 20*uiScale, fontLabel, RED);
    }

    if (carDatabase[currentSelection].hasUpgrade) {
        int cx = screenW / 2;
        int cy = screenH - (int)(100*uiScale);
        
        if (viewingUpgrade) {
             const char* txt = "Viewing: SPORT / LUXURY Model";
             DrawText(txt, cx - MeasureText(txt, fontValue)/2, cy, fontValue, GOLD);
             const char* sub = "(Press UP/DOWN for Base Model)";
             DrawText(sub, cx - MeasureText(sub, fontLabel)/2, cy + (int)(30*uiScale), fontLabel, LIGHTGRAY);
        } else {
             const char* txt = "Viewing: BASE Model";
             DrawText(txt, cx - MeasureText(txt, fontValue)/2, cy, fontValue, WHITE);
             const char* sub = "(Press UP/DOWN for Upgraded Model)";
             DrawText(sub, cx - MeasureText(sub, fontLabel)/2, cy + (int)(30*uiScale), fontLabel, GOLD);
        }
    }
}

//
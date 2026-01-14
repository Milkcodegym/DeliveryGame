/*
 * -----------------------------------------------------------------------------
 * Game Title: Delivery Game
 * Authors: Lucas Liço, Michail Michailidis
 * Copyright (c) 2025-2026
 * License: zlib/libpng
 * -----------------------------------------------------------------------------
 */

#include "player.h"
#include "raymath.h" 
#include "save.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BAR_WIDTH 220
#define BAR_HEIGHT 25
#define BAR_MARGIN_X 20
#define BAR_MARGIN_Y 20

// --- HELPER FUNCTIONS ---

void AddMoney(Player *player, const char* desc, float amount) {
    player->money += amount;
    int max = (player->transactionCount < MAX_TRANSACTIONS) ? player->transactionCount : MAX_TRANSACTIONS - 1;
    for (int i = max; i > 0; i--) player->history[i] = player->history[i-1];
    Transaction t;
    strncpy(t.description, desc, 31); t.description[31] = '\0';
    t.amount = amount;
    player->history[0] = t;
    if (player->transactionCount < MAX_TRANSACTIONS) player->transactionCount++;
}

void LoadPlayerContent(Player *player) {
    char path[128];
    if (strlen(player->currentModelFileName) > 0) sprintf(path, "resources/Playermodels/%s", player->currentModelFileName);
    else sprintf(path, "resources/Playermodels/sedan.obj");
    player->model = LoadModel(path);
}

// --- PHYSICS HELPERS ---

void ResolveMovement(Player* player, GameMap* map, TrafficManager* traffic, float moveAmount, int axis) {
    float testX = player->position.x;
    float testZ = player->position.z;

    if (axis == 1) testX += moveAmount;
    else           testZ += moveAmount;

    // Check Map Collision 
    bool hitMap = CheckMapCollision(map, testX, testZ, player->radius, 0);
    Vector3 hitCar = TrafficCollision(traffic, testX, testZ, player->radius);

    // 1. Path Clear? Move.
    if (!hitMap && hitCar.z == -1) {
        if (axis == 1) player->position.x += moveAmount;
        else           player->position.z += moveAmount;
        return; 
    }

    // 2. Car Crash? Bounce.
    if (hitCar.z != -1) {
        float trafficSpeed = hitCar.z;
        float impactSpeed = fabsf(player->current_speed - trafficSpeed);
        if (impactSpeed > 4.0f) {
            int damage = (int)((impactSpeed - 4.0f) * 3.0f);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }
        player->current_speed *= -0.4f; 
    } 
    // 3. Wall Crash? HARD STOP.
    else {
        // Impact Damage
        if (fabs(player->current_speed) > 8.0f) { 
            int damage = (int)((fabs(player->current_speed) - 8.0f) * 3.0f);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }
        
        // Kill momentum instantly (No sliding)
        player->current_speed = 0.0f;
    }
}

// --- MAIN FUNCTIONS ---

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    
    p.radius = 1.6f;  // Sedan Radius
    p.position.y = 0.4f; 

    p.health = 100.0f;
    p.current_speed = 0.0f;
    
    // [TUNING - UPDATED FOR BETTER COASTING/BRAKING]
    p.max_speed = 22.0f;       // Reduced slightly for control
    p.acceleration = 1.0f;     
    p.brake_power = 22.0f;     // [FIX] Very strong brakes (Was 4.5)
    p.turn_speed = 3.0f;       
    
    // [FIX] Friction set to 0.999 allows "Rolling"
    // Previously 0.98 meant 2% speed loss PER FRAME (huge drag)
    p.friction = 0.992f;        
    
    p.drag = 0.002f;           // Very low air drag
    p.steering_val = 0.0f;     
    p.pinGForce=true;
    p.pinFuel=true;
    p.pinThermometer=true;
    p.pinSpeed=true;
    p.rotationSpeed = 120.0f; 
    
    p.yVelocity = 0.0f;
    p.isGrounded = true; 
    p.angle = 0.0f;

    // Initialize Garage
    for(int i=0; i<10; i++) {
        p.ownedCars[i] = false;
        p.ownedUpgrades[i] = false; // [NEW]
    }
    
    // Default Car (Sedan Base)
    p.ownedCars[1] = true; 
    p.currentCarIndex = 1; 
    p.isDrivingUpgrade = false; // [NEW] Driving Base Model 
    
    strcpy(p.currentModelFileName, "sedan.obj");
    p.maxFuel = 100.0f;
    p.fuel = 100.0f;
    p.fuelConsumption = 0.04f;
    p.insulationFactor = 0.0f;
    p.loadResistance = 0.0f;
    
    p.money = 0.0f;
    p.transactionCount = 0;
    AddMoney(&p, "Initial Funds", 50.00f);
    
    return p;
}

void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt) {
    if (dt > 0.04f) dt = 0.04f; 
    
    bool inputBlocked = IsMapsAppTyping();

    // 2. STEERING LOGIC
    float steerInput = 0.0f;
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) steerInput = 1.0f;
        if (IsKeyDown(KEY_D)) steerInput = -1.0f;
    }

    if (steerInput != 0.0f) {
        player->steering_val += steerInput * 4.0f * dt;
    } else {
        if (player->steering_val > 0) {
            player->steering_val -= 4.0f * dt;
            if (player->steering_val < 0) player->steering_val = 0;
        } else if (player->steering_val < 0) {
            player->steering_val += 4.0f * dt;
            if (player->steering_val > 0) player->steering_val = 0;
        }
    }
    if (player->steering_val > 1.0f) player->steering_val = 1.0f;
    if (player->steering_val < -1.0f) player->steering_val = -1.0f;

    bool attemptingMove = !inputBlocked && (IsKeyDown(KEY_W) || IsKeyDown(KEY_S));
    
    if (fabs(player->current_speed) > 0.1f || attemptingMove) {
        float turnFactor = player->steering_val * player->turn_speed * dt * 50.0f;
        player->angle += turnFactor;
    }

    // 3. Throttle Logic
    float target_speed = 0.0f;
    bool isBraking = false;

    if (!inputBlocked && player->fuel > 0) {
        if (IsKeyDown(KEY_W)) {
            target_speed = player->max_speed;
            // If moving backwards and press W, we are braking to a stop first
            if (player->current_speed < -0.1f) isBraking = true;
        }
        else if (IsKeyDown(KEY_S)) {
            target_speed = -player->max_speed * 0.5f; 
            // If moving forward and press S, we are braking
            if (player->current_speed > 0.1f) isBraking = true;
        }
    }

    // --- PHYSICS INTEGRATION ---

    // A. BRAKING (High Friction Mode)
    // We apply this logic if the player is actively trying to stop
    if (isBraking) {
        if (player->current_speed > 0) {
            player->current_speed -= player->brake_power * dt;
            if (player->current_speed < 0) player->current_speed = 0; 
        }
        else if (player->current_speed < 0) {
            player->current_speed += player->brake_power * dt;
            if (player->current_speed > 0) player->current_speed = 0;
        }
    }
    // B. COASTING (Low Friction Mode)
    // No gas, no brake. Just rolling.
    else if (target_speed == 0.0f) {
        player->current_speed *= player->friction; // 0.999f (Preserves speed)
        
        // Minor Air Drag
        float dragForce = player->current_speed * player->current_speed * player->drag * dt;
        if (player->current_speed > 0) player->current_speed -= dragForce;
        else player->current_speed += dragForce;

        if (fabs(player->current_speed) < 0.1f) player->current_speed = 0.0f;
    }
    // C. ACCELERATING
    else {
        if (player->current_speed < target_speed) {
            player->current_speed += player->acceleration * dt;
            if (player->current_speed > target_speed) player->current_speed = target_speed;
        } 
        else if (player->current_speed > target_speed) {
            player->current_speed -= player->acceleration * dt;
            if (player->current_speed < target_speed) player->current_speed = target_speed;
        }
    }

    // 4. Movement Calculation
    float moveDist = player->current_speed * dt;

    if (moveDist > 2.0f) moveDist = 2.0f;
    if (moveDist < -2.0f) moveDist = -2.0f;

    float moveX = sinf(player->angle * DEG2RAD) * moveDist;
    float moveZ = cosf(player->angle * DEG2RAD) * moveDist;

    Vector3 startPos = player->position;

    // 5. Collision & Integration
    ResolveMovement(player, map, traffic, moveX, 1);
    ResolveMovement(player, map, traffic, moveZ, 0);

    // 6. Y-Axis Sanity Check
    if (fabsf(player->position.y - startPos.y) > 0.1f) {
        player->position = startPos; 
        player->current_speed = 0.0f; 
    } else {
        player->position.y = 0.4f; 
    }
    
    player->yVelocity = 0.0f;
    player->isGrounded = true;

    // 7. Fuel Consumption
    if (fabs(moveDist) > 0.001f && player->fuel > 0) {
        player->fuel -= fabs(moveDist) * player->fuelConsumption;
        if (player->fuel < 0) player->fuel = 0;
    }
}

void DrawHealthBar(Player *player) {
    int screenWidth = GetScreenWidth();
    float barX = (float)(screenWidth - BAR_WIDTH - BAR_MARGIN_X);
    float barY = (float)BAR_MARGIN_Y;
    Rectangle bgRect = { barX, barY, BAR_WIDTH, BAR_HEIGHT };
    
    float healthPercent = (float)player->health / 100.0f;
    if (healthPercent < 0.0f) healthPercent = 0.0f;
    if (healthPercent > 1.0f) healthPercent = 1.0f;
    Rectangle healthRect = { barX, barY, BAR_WIDTH * healthPercent, BAR_HEIGHT };

    DrawRectangleRounded(bgRect, 0.5f, 10, Fade(BLACK, 0.5f));
    if (player->health > 0) DrawRectangleRounded(healthRect, 0.5f, 10, Fade(RED, 0.8f));
    DrawRectangleRoundedLines(bgRect, 0.5f, 10, Fade(WHITE, 0.5f));

    const char *text;
    if (player->health >= 99.0f) text = "HEALTH: 100%";
    else text = TextFormat("HEALTH: %d%%", (int)player->health);
    DrawText(text, (int)barX+10, (int)(barY + 5), 20, WHITE);
}
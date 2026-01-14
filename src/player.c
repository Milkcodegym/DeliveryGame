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

/*
 * Description: Adds money to the player's wallet and records the transaction in history.
 * Parameters:
 * - player: Pointer to the Player struct.
 * - desc: Description of the transaction.
 * - amount: Amount to add (or subtract if negative).
 * Returns: None.
 */
void AddMoney(Player *player, const char* desc, float amount) {
    player->money += amount;
    int max = (player->transactionCount < MAX_TRANSACTIONS) ? player->transactionCount : MAX_TRANSACTIONS - 1;
    
    // Shift history
    for (int i = max; i > 0; i--) {
        player->history[i] = player->history[i-1];
    }

    Transaction t;
    strncpy(t.description, desc, 31); 
    t.description[31] = '\0';
    t.amount = amount;
    
    player->history[0] = t;
    if (player->transactionCount < MAX_TRANSACTIONS) player->transactionCount++;
}

/*
 * Description: Loads the 3D model for the player's current vehicle.
 * Parameters:
 * - player: Pointer to the Player struct.
 * Returns: None.
 */
void LoadPlayerContent(Player *player) {
    char path[128];
    if (strlen(player->currentModelFileName) > 0) {
        sprintf(path, "resources/Playermodels/%s", player->currentModelFileName);
    } else {
        sprintf(path, "resources/Playermodels/sedan.obj");
    }
    player->model = LoadModel(path);
}

// --- PHYSICS HELPERS ---

/*
 * Description: Moves the player along a specific axis, checking for collisions with the map or traffic.
 * Parameters:
 * - player: Pointer to the Player.
 * - map: Pointer to the GameMap.
 * - traffic: Pointer to the TrafficManager.
 * - moveAmount: Distance to move.
 * - axis: 1 for X-axis, 0 for Z-axis.
 * Returns: None.
 */
void ResolveMovement(Player* player, GameMap* map, TrafficManager* traffic, float moveAmount, int axis) {
    float testX = player->position.x;
    float testZ = player->position.z;

    if (axis == 1) testX += moveAmount;
    else           testZ += moveAmount;

    // Check Collisions
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
            int damage = (int)((impactSpeed - 3.0f) * 8.0f);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }
        player->current_speed *= -0.4f; 
    } 
    // 3. Wall Crash? Hard Stop.
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

/*
 * Description: Initializes a new Player instance with default physics and economy values.
 * Parameters:
 * - startPos: The starting 3D position vector.
 * Returns: A fully initialized Player struct.
 */
Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    
    p.radius = 1.6f;
    p.position.y = 0.4f; 

    p.health = 100.0f;
    p.current_speed = 0.0f;
    
    // Physics defaults
    p.max_speed = 18.0f;       
    p.acceleration = 1.0f;     
    p.brake_power = 3.8f;      
    p.turn_speed = 1.8f;       
    p.friction = 0.995f;       
    p.drag = 0.002f;           
    
    p.steering_val = 0.0f;     
    p.rotationSpeed = 120.0f; 
    p.pinGForce = true;
    p.pinFuel = true;
    p.pinThermometer = true;
    p.pinSpeed = true;
    p.yVelocity = 0.0f;
    p.isGrounded = true; 
    p.angle = 0.0f;

    // Initialize Garage
    for(int i = 0; i < 10; i++) {
        p.ownedCars[i] = false;
        p.ownedUpgrades[i] = false;
    }
    
    p.ownedCars[1] = true; 
    p.currentCarIndex = 1; 
    p.isDrivingUpgrade = false;
    
    strcpy(p.currentModelFileName, "sedan.obj");
    p.maxFuel = 100.0f;
    p.fuel = 100.0f;
    p.fuelConsumption = 0.04f;
    
    p.money = 0.0f;
    p.transactionCount = 0;
    AddMoney(&p, "Initial Funds", 100.00f);
    
    return p;
}



/*
 * Description: Updates player physics, inputs, movement, and fuel consumption for the current frame.
 * Parameters:
 * - player: Pointer to the Player.
 * - map: Pointer to the GameMap.
 * - traffic: Pointer to the TrafficManager.
 * - dt: Delta Time.
 * Returns: None.
 */
void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt) {
    if (dt > 0.04f) dt = 0.04f; 
    
    bool inputBlocked = IsMapsAppTyping();

    // Force Physics Constants to defaults if needed
    player->friction = 0.995f; 
    if (player->brake_power > 12.0f) player->brake_power = 12.0f; 
    if (player->brake_power < 3.0f) player->brake_power = 3.0f;   

    // 1. STEERING
    float steerInput = 0.0f;
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) steerInput = 1.0f;
        if (IsKeyDown(KEY_D)) steerInput = -1.0f;
    }

    if (steerInput != 0.0f) {
        player->steering_val += steerInput * 4.0f * dt;
    } else {
        // Auto-center
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

    // Allow steering if moving OR trying to move (stuck fix)
    bool attemptingMove = !inputBlocked && (IsKeyDown(KEY_W) || IsKeyDown(KEY_S));
    if (fabs(player->current_speed) > 0.1f || attemptingMove) {
        float turnFactor = player->steering_val * player->turn_speed * dt * 50.0f;
        player->angle += turnFactor;
    }

    // 2. THROTTLE & BRAKE LOGIC
    float accel = player->acceleration;
    float brake = player->brake_power;
    float friction = player->friction;
    
    bool gas = !inputBlocked && IsKeyDown(KEY_W);
    bool reverse = !inputBlocked && IsKeyDown(KEY_S);
    
    // A. GAS (W)
    if (gas) {
        if (player->current_speed >= -0.5f) {
            // Normal Acceleration
            player->current_speed += accel * dt;
        } else {
            // Braking (while going backwards)
            player->current_speed += brake * dt;
            if (player->current_speed > 0.0f) player->current_speed = 0.0f;
        }
    }
    // B. REVERSE / BRAKE (S)
    else if (reverse) {
        if (player->current_speed > 0.5f) {
            // Braking (while going forward)
            player->current_speed -= brake * dt;
            if (player->current_speed < 0.0f) player->current_speed = 0.0f;
        } else {
            // Reversing (slower acceleration)
            player->current_speed -= (accel * 0.5f) * dt; 
        }
    }
    // C. COASTING (No Input)
    else {
        player->current_speed *= friction;
        if (fabs(player->current_speed) < 0.2f) player->current_speed = 0.0f;
    }

    // D. LIMITS
    float maxFwd = player->max_speed;
    float maxRev = -player->max_speed * 0.4f; 
    
    if (player->current_speed > maxFwd) player->current_speed = maxFwd;
    if (player->current_speed < maxRev) player->current_speed = maxRev;

    // 3. MOVEMENT APPLICATION
    float moveDist = player->current_speed * dt;
    
    // Safety cap to prevent tunneling
    if (moveDist > 1.5f) moveDist = 1.5f;
    if (moveDist < -1.5f) moveDist = -1.5f;

    float moveX = sinf(player->angle * DEG2RAD) * moveDist;
    float moveZ = cosf(player->angle * DEG2RAD) * moveDist;

    Vector3 startPos = player->position;

    ResolveMovement(player, map, traffic, moveX, 1);
    ResolveMovement(player, map, traffic, moveZ, 0);

    // Keep car grounded
    if (fabsf(player->position.y - startPos.y) > 0.1f) {
        player->position = startPos; 
        player->current_speed = 0.0f; 
    } else {
        player->position.y = 0.4f; 
    }
    player->isGrounded = true;

    // 4. FUEL
    if (fabs(moveDist) > 0.001f && player->fuel > 0) {
        player->fuel -= fabs(moveDist) * player->fuelConsumption;
        if (player->fuel < 0) player->fuel = 0;
    }
}

/*
 * Description: Renders the health bar UI element.
 * Parameters:
 * - player: Pointer to the Player.
 * Returns: None.
 */
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
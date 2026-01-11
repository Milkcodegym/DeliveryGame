#include "player.h"
#include "maps_app.h"
#include "save.h"
#include "raymath.h" 
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
    else sprintf(path, "resources/Playermodels/delivery.obj");
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
    // 3. Wall Crash? Stop.
    else {
        if (fabs(player->current_speed) > 8.0f) { 
            int damage = (int)((fabs(player->current_speed) - 8.0f) * 3.0f);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }
        player->current_speed = 0;
    }
}

// --- MAIN FUNCTIONS ---

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    
    // [FIX] Force spawn height to match the flat map logic
    p.radius = 0.4f; 
    p.position.y = p.radius; 

    p.health = 100.0f;
    p.current_speed = 0.0f;
    
    // [TUNING]
    p.friction = 2.0f;      
    p.acceleration = 15.0f; 
    p.max_speed = 22.0f;
    p.brake_power = 15.0f; 
    p.rotationSpeed = 120.0f;
    
    p.yVelocity = 0.0f;
    p.isGrounded = true; // Always grounded on flat map
    p.angle = 0.0f;

    strcpy(p.currentModelFileName, "delivery.obj"); 
    p.maxFuel = 80.0f;
    p.fuelConsumption = 0.03f;
    p.insulationFactor = 1.0f;
    p.loadResistance = 1.0f;
    p.pinSpeed = 1;
    p.money = 0.0f;
    p.transactionCount = 0;
    AddMoney(&p, "Initial Funds", 50.00f);
    AddMoney(&p, "DEVELOPER MONEY", 50450.00f);
    p.fuel = p.maxFuel * 0.85f;
    return p;
}

void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt) {
    // 1. Cap dt to prevent "Spiral of Death" at very low FPS
    if (dt > 0.04f) dt = 0.04f; 
    
    bool inputBlocked = IsMapsAppTyping();

    // 2. Rotation
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) player->angle += player->rotationSpeed * dt;
        if (IsKeyDown(KEY_D)) player->angle -= player->rotationSpeed * dt;
    }

    // 3. Throttle Logic
    float target_speed = 0.0f;
    bool isBraking = false;

    if (!inputBlocked && player->fuel > 0) {
        if (IsKeyDown(KEY_W)) {
            target_speed = player->max_speed;
            if (player->current_speed < -0.1f) isBraking = true;
        }
        else if (IsKeyDown(KEY_S)) {
            target_speed = -player->max_speed * 0.5f; 
            if (player->current_speed > 0.1f) isBraking = true;
        }
    }

    // --- PHYSICS INTEGRATION ---

    // A. COASTING
    if (target_speed == 0.0f) {
        if (player->current_speed > 0) {
            player->current_speed -= player->friction * dt;
            if (player->current_speed < 0) player->current_speed = 0;
        } 
        else if (player->current_speed < 0) {
            player->current_speed += player->friction * dt;
            if (player->current_speed > 0) player->current_speed = 0;
        }
    }
    // B. BRAKING
    else if (isBraking) {
        if (player->current_speed > 0) {
            player->current_speed -= player->brake_power * dt;
            if (player->current_speed < 0) player->current_speed = 0; 
        }
        else if (player->current_speed < 0) {
            player->current_speed += player->brake_power * dt;
            if (player->current_speed > 0) player->current_speed = 0;
        }
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

    // Safety Cap: 2.0m per frame limit (Prevents tunneling)
    if (moveDist > 2.0f) moveDist = 2.0f;
    if (moveDist < -2.0f) moveDist = -2.0f;

    float moveX = sinf(player->angle * DEG2RAD) * moveDist;
    float moveZ = cosf(player->angle * DEG2RAD) * moveDist;

    // [CRITICAL] Store valid state before applying physics
    Vector3 startPos = player->position;

    // 5. Collision & Integration
    // We intentionally REMOVED gravity. The car glides on the X/Z plane.
    ResolveMovement(player, map, traffic, moveX, 1);
    ResolveMovement(player, map, traffic, moveZ, 0);

    // 6. [NEW] Y-AXIS SANITY CHECK
    // If the physics calculation somehow moved the player vertically (glitch/lag),
    // we assume the whole frame is corrupted and REVERT everything.
    // Tolerance is 0.1f to allow for minor floating point drift.
    if (fabsf(player->position.y - startPos.y) > 0.1f) {
        player->position = startPos;  // Undo X/Z movement too (Block the illegal move)
        player->current_speed = 0.0f; // Kill momentum safely
    } else {
        // Enforce exact ground height (Flat Map Logic)
        player->position.y = player->radius; 
    }
    
    player->yVelocity = 0.0f;
    player->isGrounded = true;

    // 7. Fuel
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
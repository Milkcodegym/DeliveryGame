#include "player.h"
#include "maps_app.h"
#include "save.h"
#include "raymath.h" // Added for vector math
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- RESTORED UI CONSTANTS ---
#define BAR_WIDTH 220
#define BAR_HEIGHT 25
#define BAR_MARGIN_X 20
#define BAR_MARGIN_Y 20

// --- HELPER FUNCTIONS ---

void AddMoney(Player *player, const char* desc, float amount) {
    player->money += amount;
    int max = (player->transactionCount < MAX_TRANSACTIONS) ? player->transactionCount : MAX_TRANSACTIONS - 1;
    for (int i = max; i > 0; i--) {
        player->history[i] = player->history[i-1];
    }
    Transaction t;
    strncpy(t.description, desc, 31);
    t.description[31] = '\0';
    t.amount = amount;
    player->history[0] = t;
    if (player->transactionCount < MAX_TRANSACTIONS) {
        player->transactionCount++;
    }
}

// --- MAIN PLAYER FUNCTIONS ---

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    p.health = 100.0f;
    p.current_speed = 0.0f;
    p.friction = 1.5f;    
    p.acceleration = 1.3f;
    p.max_speed = 14.0f;
    p.brake_power = 6.0f; 
    p.rotationSpeed = 90.0f;
    p.radius = 0.3f; 
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    p.angle = 0.0f;

    // Default Model Filename (Critical for Save System)
    // Assuming default is the SUV or Van. Let's set a default safe value.
    strcpy(p.currentModelFileName, "delivery.obj"); 

    // Default Stats (Will be overwritten if loading a save)
    p.maxFuel = 80.0f;
    p.fuelConsumption = 0.04f;
    p.insulationFactor = 1.0f;
    p.loadResistance = 1.0f;

    p.money = 0.0f;
    p.transactionCount = 0;
    AddMoney(&p, "Initial Funds", 50.00f);
    AddMoney(&p, "DEVELOPER MONEY", 450.00f);

    // Init Fuel
    p.fuel = p.maxFuel * 0.45f; // Start with ~45% to demonstrate refueling

    return p;
}

void LoadPlayerContent(Player *player) {
    // If a model filename was loaded from save, use it. Otherwise default.
    char path[128];
    if (strlen(player->currentModelFileName) > 0) {
        sprintf(path, "resources/Playermodels/%s", player->currentModelFileName);
    } else {
        strcpy(player->currentModelFileName, "delivery.obj");
        sprintf(path, "resources/Playermodels/delivery.obj");
    }
    
    player->model = LoadModel(path);
}

bool checkcamera_collision=false;

void ResolveMovement(Player* player, GameMap* map, TrafficManager* traffic, float moveAmount, int axis) {
    
    // 1. Calculate the Test Position based on the axis
    float testX = player->position.x;
    float testZ = player->position.z;

    if (axis == 1) testX += moveAmount;
    else           testZ += moveAmount;

    // 2. Run Collision Checks
    bool hitMap = CheckMapCollision(map, testX, testZ, player->radius);
    Vector3 hitCar = TrafficCollision(traffic, testX, testZ, player->radius);

    // 3. LOGIC: If path is clear
    if (!hitMap && hitCar.z == -1) {
        // Apply movement to the correct axis
        if (axis == 1) player->position.x += moveAmount;
        else                player->position.z += moveAmount;
        return; // We are done, exit function
    }

    // 4. LOGIC: Car Collision
    if (hitCar.z != -1) {
        float trafficSpeed = hitCar.z;

        // --- CALCULATION ---
        float pDirX = sinf(player->angle * DEG2RAD);
        float pDirZ = cosf(player->angle * DEG2RAD);
        float tDirX = hitCar.x;
        float tDirZ = hitCar.y;

        float alignment = (pDirX * tDirX) + (pDirZ * tDirZ);
        float impactSpeed = 0.0f;

        if (alignment < -0.2f) {
            // Head-On
            impactSpeed = player->current_speed + trafficSpeed;
            player->current_speed *= -0.5f;
        } else {
            // Rear-end/Side
            impactSpeed = fabsf(player->current_speed - trafficSpeed);
            player->current_speed *= 0.5f;
        }

        // --- DAMAGE ---
        if (impactSpeed > 2.0f) {
            int damage = (int)((impactSpeed - 2.0f) * 5.0f);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }

        // --- BOUNCE ---
        player->current_speed *= -0.5f;
        
        // Push player back slightly to avoid sticking
        if (axis == 1) player->position.x -= moveAmount;
        else                player->position.z -= moveAmount;
    } 
    // 5. LOGIC: Map Collision
    else {
        if (player->current_speed > 2) {
            int damage = (int)((player->current_speed - 2) * 10);
            player->health -= damage;
            if (player->health < 0) player->health = 0;
        }
        player->current_speed = 0;
    }
}

void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    bool inputBlocked = IsMapsAppTyping();

    Vector3 move = { 0 };
    float angle=0.0f;
    // --- 1. Rotation ---
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) angle += player->rotationSpeed * dt;
        if (IsKeyDown(KEY_D)) angle -= player->rotationSpeed * dt;
        player->angle += angle;
    }

    // --- 2. Determine Target Speed ---
    float target_speed = 0.0f;

    // Check fuel before allowing acceleration
    if (!inputBlocked && player->fuel > 0) {
        if (IsKeyDown(KEY_W)) {
            target_speed = player->max_speed;
            checkcamera_collision = true;
        }
        if (IsKeyDown(KEY_S)){
            if (target_speed==player->max_speed) target_speed = 0;
            else target_speed = -player->max_speed;
        }
    }

    if (player->current_speed < target_speed) {
        player->current_speed += player->acceleration * dt;
        if (player->current_speed > target_speed) player->current_speed = target_speed;
        
        if ((player->current_speed < 0) && (target_speed > 0)) {
            player->current_speed += player->acceleration * dt * (player->brake_power - 1);
        }
        if (target_speed == 0) {
            player->current_speed += player->acceleration * dt * (player->friction - 1);
        }
    } 
    else if (player->current_speed > target_speed) {
        player->current_speed -= player->acceleration * dt;
        if (player->current_speed < target_speed) player->current_speed = target_speed;
        
        if ((player->current_speed > 0) && (target_speed < 0)) {
            player->current_speed -= player->acceleration * dt * (player->brake_power - 1);
        }
        if (target_speed == 0) {
            player->current_speed -= player->acceleration * dt * (player->friction - 1);
        }
    }

    // --- 3. Apply Movement ---
    float moveDist = player->current_speed * dt;
    move.x += sinf(player->angle * DEG2RAD) * moveDist;
    move.z += cosf(player->angle * DEG2RAD) * moveDist;

    // [UPDATED] Dynamic Fuel Consumption using Player Stats
    if (fabs(moveDist) > 0.001f && player->fuel > 0) {
        float consumption = player->fuelConsumption;
        if (consumption <= 0.0f) consumption = 0.01f; // Safety fallback
        
        player->fuel -= fabs(moveDist) * consumption;
        if (player->fuel < 0) player->fuel = 0;
    } 
    // Out of fuel deceleration logic
    else if (player->fuel <= 0 && fabs(player->current_speed) > 0) {
        if (player->current_speed > 0) player->current_speed -= 5.0f * dt;
        else player->current_speed += 5.0f * dt;
        
        if (fabs(player->current_speed) < 0.5f) player->current_speed = 0;
    }

    // --- 4. Gravity ---
    player->yVelocity -= 20.0f * dt;
    
    player->position.y += player->yVelocity * dt;

    if (player->position.y <= player->radius) {
        player->position.y = player->radius;
        player->yVelocity = 0;
        player->isGrounded = true;
    } else {
        player->isGrounded = false;
    }

    // --- 5 & 6. UNIFIED COLLISION DETECTION ---
    ResolveMovement(player, map, traffic, move.x, 1);
    ResolveMovement(player, map, traffic, move.z, 0);
}

void DrawHealthBar(Player *player) {
    int screenWidth = GetScreenWidth();
    
    // 1. Calculate Positions
    float barX = (float)(screenWidth - BAR_WIDTH - BAR_MARGIN_X);
    float barY = (float)BAR_MARGIN_Y;
    
    // 2. Define Rectangles
    Rectangle bgRect = { barX, barY, BAR_WIDTH, BAR_HEIGHT };
    
    float healthPercent = (float)player->health / 100.0f;
    if (healthPercent < 0.0f) healthPercent = 0.0f;
    if (healthPercent > 1.0f) healthPercent = 1.0f;
    
    Rectangle healthRect = { barX, barY, BAR_WIDTH * healthPercent, BAR_HEIGHT };

    // 3. Draw
    DrawRectangleRounded(bgRect, 0.5f, 10, Fade(BLACK, 0.5f));

    if (player->health > 0) {
        DrawRectangleRounded(healthRect, 0.5f, 10, Fade(RED, 0.8f));
    }

    DrawRectangleRoundedLines(bgRect, 0.5f, 10, Fade(WHITE, 0.5f));

    const char *text;
    if (player->health == 100){text = "HEALTH POINTS : 100";}
    else {text = TextFormat("HEALTH POINTS : %3.1f", player->health);}
    
    DrawText(text, (int)barX-3, (int)(barY + BAR_HEIGHT + 5), 20, RED);
}
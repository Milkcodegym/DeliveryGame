#include "player.h"
#include "maps_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
    
    p.current_speed = 0.0f;
    p.friction = 1.5f;    
    p.acceleration = 1.3f;
    p.max_speed = 12.0f;
    p.brake_power = 6.0f; 
    p.rotationSpeed = 90.0f;
    p.radius = 0.3f; // Set a small radius for collision (e.g., 0.3f)
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    p.angle = 0.0f;

    p.money = 0.0f;
    p.transactionCount = 0;
    AddMoney(&p, "Initial Funds", 50.00f);

    return p;
}

void LoadPlayerContent(Player *player) {
    player->model = LoadModel("resources/vehicle-suv.obj");
}

bool checkcamera_collision=false;

void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    bool inputBlocked = IsMapsAppTyping();

    Vector3 move = { 0 };

    // --- 1. Rotation ---
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) player->angle += player->rotationSpeed * dt;
        if (IsKeyDown(KEY_D)) player->angle -= player->rotationSpeed * dt;
    }

    // --- 2. Determine Target Speed ---
    float target_speed = 0.0f;

    if (!inputBlocked) {
        if (IsKeyDown(KEY_W)) {
            target_speed = player->max_speed;
            checkcamera_collision = true;
        }
        if (IsKeyDown(KEY_S)) target_speed = -player->max_speed;
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
    move.x += sinf(player->angle * DEG2RAD) * player->current_speed * dt;
    move.z += cosf(player->angle * DEG2RAD) * player->current_speed * dt;

    // --- 4. Gravity ---
    player->yVelocity -= 20.0f * dt;
    
    if (player->isGrounded && IsKeyPressed(KEY_SPACE)) {
        player->yVelocity = 8.0f;
        player->isGrounded = false;
    }
    
    player->position.y += player->yVelocity * dt;

    if (player->position.y <= player->radius) {
        player->position.y = player->radius;
        player->yVelocity = 0;
        player->isGrounded = true;
    } else {
        player->isGrounded = false;
    }

    // --- 5. Horizontal Collision (FIXED) ---
    // Added 'player->radius' as the 4th argument
    if (!CheckMapCollision(map, player->position.x + move.x, player->position.z, player->radius)) {
        player->position.x += move.x;
    } else {
        player->current_speed = 0;
    }
    
    if (!CheckMapCollision(map, player->position.x, player->position.z + move.z, player->radius)) {
        player->position.z += move.z;
    } else {
        player->current_speed = 0;
    }

    float putback=0.1f;
    // --- 6. Collision with Traffic ---
    if (!TrafficCollision(traffic, player->position.x + move.x, player->position.z, player->radius)) {
        player->position.x += move.x;
    }
    else {
        player->current_speed = 0;
        player->position.x -= putback;
    }

    if (!TrafficCollision(traffic, player->position.x, player->position.z + move.z, player->radius)) {
        player->position.z += move.z;
    }
    else {
        player->current_speed = 0;
        player->position.z -= putback;
    }
}
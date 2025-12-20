#include "player.h"
#include "maps_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h> // Required for the new phone/money logic

// --- HELPER FUNCTIONS (From New Version) ---

// Helper: Adds money and logs to history (Newest at index 0)
void AddMoney(Player *player, const char* desc, float amount) {
    player->money += amount;
    
    // 1. Shift existing transactions down by 1 (Oldest falls off if full)
    int max = (player->transactionCount < MAX_TRANSACTIONS) ? player->transactionCount : MAX_TRANSACTIONS - 1;
    for (int i = max; i > 0; i--) {
        player->history[i] = player->history[i-1];
    }

    // 2. Insert new transaction at top
    Transaction t;
    strncpy(t.description, desc, 31);
    t.description[31] = '\0'; // Safety null
    t.amount = amount;
    player->history[0] = t;

    // 3. Increment count if not full
    if (player->transactionCount < MAX_TRANSACTIONS) {
        player->transactionCount++;
    }
}

// --- MAIN PLAYER FUNCTIONS ---

Player InitPlayer(Vector3 startPos) {
    Player p = {0};
    p.position = startPos;
    
    // Physics Init (Restored your original values)
    p.current_speed = 0.0f;
    p.friction = 9.0f;    // Needs to be over 1
    p.acceleration = 1.3f;
    p.max_speed = 12.0f;
    p.brake_power = 6.0f; // Needs to be over 1
    p.rotationSpeed = 90.0f;
    p.radius = 0.3f;
    p.yVelocity = 0.0f;
    p.isGrounded = false;
    p.angle = 0.0f;

    // Economy Init (From New Version)
    p.money = 0.0f;
    p.transactionCount = 0;
    
    // Add initial funds using our helper
    AddMoney(&p, "Initial Funds", 50.00f);

    return p;
}

void LoadPlayerContent(Player *player) {
    player->model = LoadModel("resources/vehicle-suv.obj");
}

void UpdatePlayer(Player *player, GameMap *map, float dt) {
    // --- SAFETY FIX: Clamp Delta Time ---
    if (dt > 0.1f) dt = 0.1f;
    bool inputBlocked = IsMapsAppTyping();

    Vector3 move = { 0 };

    // --- 1. Rotation ---
    if (!inputBlocked) {
        if (IsKeyDown(KEY_A)) player->angle += player->rotationSpeed * dt;
        if (IsKeyDown(KEY_D)) player->angle -= player->rotationSpeed * dt;
    }

    // --- 2. Determine Target Speed (RESTORED OLD LOGIC) ---
    float target_speed = 0.0f;

    if (!inputBlocked) {
        if (IsKeyDown(KEY_W)) target_speed = player->max_speed;
        if (IsKeyDown(KEY_S)) target_speed = -player->max_speed;
    }

    if (player->current_speed < target_speed) {
        // We need to speed up (or reverse from negative)
        player->current_speed += player->acceleration * dt;
        
        // Cap it so we don't overshoot the target
        if (player->current_speed > target_speed) player->current_speed = target_speed;
        
        // Braking logic (Reversing direction)
        if ((player->current_speed < 0) && (target_speed > 0)) {
            player->current_speed += player->acceleration * dt * (player->brake_power - 1);
        }
        
        // Friction logic (Coasting)
        if (target_speed == 0) { // Fixed typo: changed = to ==
            player->current_speed += player->acceleration * dt * (player->friction - 1);
        }
    } 
    else if (player->current_speed > target_speed) {
        // We need to slow down (or reverse from positive)
        player->current_speed -= player->acceleration * dt;
        
        // Cap it so we don't overshoot the target
        if (player->current_speed < target_speed) player->current_speed = target_speed;
        
        // Braking logic (Reversing direction)
        if ((player->current_speed > 0) && (target_speed < 0)) {
            player->current_speed -= player->acceleration * dt * (player->brake_power - 1);
        }
        
        // Friction logic (Coasting)
        if (target_speed = 0) { // EDW EXEIS LATHOS ALLA EINAI KALYTERA ME TO LATHOS LARF
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

    // Floor Collision
    if (player->position.y <= player->radius) {
        player->position.y = player->radius;
        player->yVelocity = 0;
        player->isGrounded = true;
    } else {
        player->isGrounded = false;
    }

    // --- 5. Horizontal Collision ---
    if (!CheckMapCollision(map, player->position.x + move.x, player->position.z, player->radius)) {
        player->position.x += move.x;
    }
    if (!CheckMapCollision(map, player->position.x, player->position.z + move.z, player->radius)) {
        player->position.z += move.z;
    }
}
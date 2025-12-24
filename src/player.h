#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "map.h"
#include "traffic.h"

#define MAX_TRANSACTIONS 10

// Constants for the Health Bar
#define BAR_WIDTH 220
#define BAR_HEIGHT 25
#define BAR_MARGIN_X 20
#define BAR_MARGIN_Y 20

// --- Economy Structures ---
typedef struct Transaction {
    char description[32];
    float amount; // Positive = Income, Negative = Expense
} Transaction;

// --- Player Structure ---
typedef struct Player {
    Vector3 position;
    
    // Physics
    float health;
    float current_speed;
    float max_speed;
    float radius;
    float rotationSpeed;
    float yVelocity;
    bool isGrounded;
    float angle;
    float acceleration;
    float brake_power;
    float friction;
    
    // Rendering
    Model model;

    // ECONOMY (New)
    float money;
    Transaction history[MAX_TRANSACTIONS];
    int transactionCount; 
    float totalEarnings;  // For "Money Made" stat
    int totalDeliveries;  // For "Total Deliveries" stat

} Player;

extern bool checkcamera_collision;
Player InitPlayer(Vector3 startPos);
void LoadPlayerContent(Player *player);
void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt);
void DrawHealthBar(Player *player);

// Helper: Safely add money and log it to history
void AddMoney(Player *player, const char* desc, float amount);

#endif
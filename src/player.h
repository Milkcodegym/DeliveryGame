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

#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "map.h"
#include "traffic.h"
#include "maps_app.h"


#define MAX_TRANSACTIONS 10
#define MAX_FUEL 100.0f

// --- Economy Structures ---
typedef struct Transaction {
    char description[32];
    float amount; 
} Transaction;

// --- Player Structure ---
typedef struct Player {
    Vector3 position;
    
    // Physics & Movement
    float health;
    float current_speed;
    float max_speed;
    float acceleration;
    float brake_power;
    float friction;
    float radius;
    float rotationSpeed;
    float yVelocity;
    bool isGrounded;
    float angle;
    // [NEW] Physics
    float drag;          // Air resistance
    float turn_speed;    // Max turning capability
    float steering_val;  // Current steering (-1.0 to 1.0)
    Vector3 prevPosition; 
    float prevAngle;
    float physicsAccumulator;
    Vector3 renderPosition; // <--- DRAW THIS ONE
    float renderAngle;
    
    // Rendering
    Model model;
    char currentModelFileName[64]; // [NEW] To save/load the correct car mesh
    bool ownedCars[10];      // Tracks Base Models
    bool ownedUpgrades[10];  // [NEW] Tracks Upgraded Models
    int currentCarIndex; 
    bool isDrivingUpgrade; // To remember what we are driving
    // ECONOMY
    float money;
    Transaction history[MAX_TRANSACTIONS];
    int transactionCount; 
    float totalEarnings;
    int totalDeliveries;

    // FUEL SYSTEM
    float fuel;
    float maxFuel;          // [SAVE THIS]
    float fuelConsumption;  // [SAVE THIS]

    // CARGO & PHYSICS LOGIC
    float insulationFactor; // [SAVE THIS]
    float loadResistance;   // [SAVE THIS]

    // [NEW] APP & UPGRADES
    bool hasCarMonitorApp;
    
    // Unlocks
    bool unlockGForce;
    bool unlockThermometer;
    
    // Visual Pins
    bool pinSpeed;
    bool pinFuel;
    bool pinAccel;
    bool pinGForce;
    bool pinThermometer;

    // GAME STATE
    bool tutorialFinished; // [NEW]

} Player;

extern bool checkcamera_collision;
Player InitPlayer(Vector3 startPos);
void LoadPlayerContent(Player *player);
void UpdatePlayer(Player *player, GameMap *map, TrafficManager *traffic, float dt);
void DrawHealthBar(Player *player);
void AddMoney(Player *player, const char* desc, float amount);

#endif
#ifndef SAVE_H
#define SAVE_H

#include "player.h"
#include "phone.h" // [FIX] This needs to be included to see PhoneSettings

#define SAVE_FILE_NAME "save_data.dat"
#define SAVE_VERSION 2  

typedef struct GameSaveData {
    int version;
    
    // Physics & Pos
    Vector3 position;
    float angle;
    
    // Car Configuration
    char modelFileName[64];
    float max_speed;
    float acceleration;
    float brake_power;
    float maxFuel;
    float fuelConsumption;
    float insulationFactor;
    float loadResistance;

    // Resource State
    float fuel;
    float health;

    // Economy
    float money;
    int totalDeliveries;
    float totalEarnings;
    int transactionCount;
    Transaction history[MAX_TRANSACTIONS];

    // Upgrades
    bool hasCarMonitorApp;
    bool unlockGForce;
    bool unlockThermometer;

    // UI State
    bool pinSpeed;
    bool pinFuel;
    bool pinAccel;
    bool pinGForce;
    bool pinThermometer;

    // Global State
    bool tutorialFinished; // [CONFIRMED] Tutorial state saving

    // Phone Settings
    PhoneSettings settings; // Now defined because phone.h is included

} GameSaveData;

bool SaveGame(Player *player, PhoneState *phone);
bool LoadGame(Player *player, PhoneState *phone);
void ResetSaveGame(Player *player, PhoneState *phone);

#endif
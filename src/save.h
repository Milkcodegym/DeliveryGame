#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include "raylib.h"
#include "player.h" // For Player struct and Transaction
#include "phone.h"  // For SettingsState

// Version control for your save file. Increment this if you change the struct later!
#define SAVE_VERSION 1 
#define SAVE_FILE_NAME "savegame.dat"

typedef struct {
    int version;
    
    // --- Player Data ---
    Vector3 position;
    float angle;
    float money;
    float fuel;
    
    // Stats
    int totalDeliveries;
    float totalEarnings;
    
    // Transaction History (Fixed size array is safe to save directly)
    Transaction history[MAX_TRANSACTIONS];
    int transactionCount;

    // Upgrades / Unlocks
    bool hasCarMonitorApp;
    bool unlockGForce;
    bool unlockThermometer;
    
    // Visual Pins
    bool pinSpeed;
    bool pinFuel;
    bool pinAccel;
    bool pinGForce;
    bool pinThermometer;

    // --- Phone/Settings Data ---
    SettingsState settings;

    // (Optional) We could save active jobs here, 
    // but for stability, we often only save "Home" state.
    // Let's stick to core progression for stability first.

} GameSaveData;

// Core Functions
bool SaveGame(Player *player, PhoneState *phone);
bool LoadGame(Player *player, PhoneState *phone);
void ResetSaveGame(Player *player, PhoneState *phone);

#endif
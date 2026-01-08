#include "save.h"
#include <stdio.h>
#include <string.h> 

const unsigned char KEY = 0xAA; 

void Obfuscate(unsigned char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= KEY;
    }
}

bool SaveGame(Player *player, PhoneState *phone) {
    GameSaveData data = {0};
    
    // 1. PACK DATA
    data.version = SAVE_VERSION;
    
    // Position
    data.position = player->position;
    data.angle = player->angle;
    
    // Car Stats (Critical for Dealership Persistence)
    strcpy(data.modelFileName, player->currentModelFileName);
    data.max_speed = player->max_speed;
    data.acceleration = player->acceleration;
    data.brake_power = player->brake_power;
    data.maxFuel = player->maxFuel;
    data.fuelConsumption = player->fuelConsumption;
    data.insulationFactor = player->insulationFactor;
    data.loadResistance = player->loadResistance;

    // Resources
    data.fuel = player->fuel;
    data.health = player->health;
    
    // Economy
    data.money = player->money;
    data.totalDeliveries = player->totalDeliveries;
    data.totalEarnings = player->totalEarnings;
    data.transactionCount = player->transactionCount;
    
    for(int i=0; i<MAX_TRANSACTIONS; i++) {
        data.history[i] = player->history[i];
    }

    // Upgrades
    data.hasCarMonitorApp = player->hasCarMonitorApp;
    data.unlockGForce = player->unlockGForce;
    data.unlockThermometer = player->unlockThermometer;
    
    // UI Pins
    data.pinSpeed = player->pinSpeed;
    data.pinFuel = player->pinFuel;
    data.pinAccel = player->pinAccel;
    data.pinGForce = player->pinGForce;
    data.pinThermometer = player->pinThermometer;

    // Game Progress
    data.tutorialFinished = player->tutorialFinished;

    // Phone
    data.settings = phone->settings;

    // 2. ENCRYPT & WRITE
    Obfuscate((unsigned char*)&data, sizeof(GameSaveData));

    FILE *file = fopen(SAVE_FILE_NAME, "wb");
    if (!file) {
        TraceLog(LOG_ERROR, "SAVE: Could not open file for writing.");
        return false;
    }

    size_t written = fwrite(&data, sizeof(GameSaveData), 1, file);
    fclose(file);

    if (written == 1) {
        TraceLog(LOG_INFO, "SAVE: Game saved successfully.");
        return true;
    } else {
        TraceLog(LOG_ERROR, "SAVE: Write failed.");
        return false;
    }
}

bool LoadGame(Player *player, PhoneState *phone) {
    FILE *file = fopen(SAVE_FILE_NAME, "rb");
    if (!file) {
        TraceLog(LOG_INFO, "SAVE: No save file found. Starting fresh.");
        return false;
    }

    GameSaveData data;
    size_t read = fread(&data, sizeof(GameSaveData), 1, file);
    fclose(file);

    if (read != 1) return false;

    // 1. DECRYPT
    Obfuscate((unsigned char*)&data, sizeof(GameSaveData));

    // 2. VERSION CHECK
    // If version mismatches, we usually reset to avoid garbage data
    if (data.version != SAVE_VERSION) {
        TraceLog(LOG_WARNING, "SAVE: Save version mismatch (Old file?). Starting fresh.");
        return false;
    }

    // 3. UNPACK DATA
    
    // Position
    player->position = data.position;
    player->angle = data.angle;
    
    // --- CAR MODEL RESTORATION ---
    // Check if the saved model is different from current default
    if (strlen(data.modelFileName) > 0) {
        TraceLog(LOG_INFO, "SAVE: Restoring vehicle model: %s", data.modelFileName);
        
        // Unload default/previous model
        UnloadModel(player->model);
        
        // Load Saved Car
        char path[128];
        sprintf(path, "resources/Playermodels/%s", data.modelFileName);
        player->model = LoadModel(path);
        
        // Store filename in player struct for next save
        strcpy(player->currentModelFileName, data.modelFileName);
        
        // Recalculate bounds
        BoundingBox box = GetModelBoundingBox(player->model);
        player->radius = (box.max.x - box.min.x) * 0.4f;
    }
    
    // Physics Constants
    player->max_speed = data.max_speed;
    player->acceleration = data.acceleration;
    player->brake_power = data.brake_power;
    player->maxFuel = data.maxFuel;
    player->fuelConsumption = data.fuelConsumption;
    player->insulationFactor = data.insulationFactor;
    player->loadResistance = data.loadResistance;

    // Resources
    player->fuel = data.fuel;
    player->health = data.health;
    
    // Economy
    player->money = data.money;
    player->totalDeliveries = data.totalDeliveries;
    player->totalEarnings = data.totalEarnings;
    player->transactionCount = data.transactionCount;
    
    for(int i=0; i<MAX_TRANSACTIONS; i++) {
        player->history[i] = data.history[i];
    }

    // Upgrades
    player->hasCarMonitorApp = data.hasCarMonitorApp;
    player->unlockGForce = data.unlockGForce;
    player->unlockThermometer = data.unlockThermometer;
    
    // Pins
    player->pinSpeed = data.pinSpeed;
    player->pinFuel = data.pinFuel;
    player->pinAccel = data.pinAccel;
    player->pinGForce = data.pinGForce;
    player->pinThermometer = data.pinThermometer;
    
    // Game State
    player->tutorialFinished = data.tutorialFinished;

    // Phone
    phone->settings = data.settings;

    TraceLog(LOG_INFO, "SAVE: Game Loaded Successfully.");
    return true;
}

void ResetSaveGame(Player *player, PhoneState *phone) {
    remove(SAVE_FILE_NAME);
    
    // Reset Critical Logic
    player->money = 0;
    player->fuel = MAX_FUEL;
    player->totalDeliveries = 0;
    player->transactionCount = 0;
    player->hasCarMonitorApp = false;
    player->tutorialFinished = false; // Reset tutorial
    
    // Reset to Default Van Stats (Hardcoded default)
    player->max_speed = 60.0f;
    player->acceleration = 0.3f;
    player->brake_power = 2.0f;
    player->maxFuel = 80.0f;
    player->fuelConsumption = 0.04f;
    
    // Note: Model reset usually requires reloading the scene or restarting app, 
    // as we don't want to load models in the middle of a reset function.
    
    TraceLog(LOG_INFO, "SAVE: Save file deleted and state reset.");
}
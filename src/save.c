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
    // 1. DELETE FILE
    if (FileExists(SAVE_FILE_NAME)) {
        remove(SAVE_FILE_NAME);
    }

    // 2. RESOURCE CLEANUP
    // Unload the current vehicle model to prevent leaks/errors
    if (player->model.meshCount > 0) {
        UnloadModel(player->model);
        player->model = (Model){0}; // Safety null
    }

    // 3. RESET PLAYER STATE (Manual Factory Reset)
    // --- Physics & Stats ---
    player->position = (Vector3){ 0.0f, 1.0f, 0.0f }; // Safe spawn height
    player->angle = 0.0f;
    player->current_speed = 0.0f;
    player->health = 100.0f;
    
    // --- Economy ---
    player->money = 50.0f;
    player->fuel = 100.0f;
    player->maxFuel = 100.0f;
    player->fuelConsumption = 0.04f;
    player->totalDeliveries = 0;
    player->totalEarnings = 0;
    player->transactionCount = 0;
    
    // --- Vehicle (Default Van Stats) ---
    player->max_speed = 22.0f;
    player->acceleration = 1.3f;
    player->brake_power = 2.0f;
    player->friction = 0.98f;
    player->radius = 1.8f;
    player->insulationFactor = 0.0f;
    player->loadResistance = 0.0f;
    
    // Load Default Model
    char path[128] = "resources/Playermodels/sedan.obj";
    if (FileExists(path)) {
        player->model = LoadModel(path);
        strcpy(player->currentModelFileName, "sedan.obj");
    }
    
    // --- Clear History ---
    for(int i=0; i<MAX_TRANSACTIONS; i++) {
        player->history[i] = (Transaction){0};
    }

    // --- Reset Upgrades ---
    player->hasCarMonitorApp = false;
    player->unlockGForce = false;
    player->unlockThermometer = false;
    player->pinSpeed = true;
    player->pinFuel = true;
    player->pinAccel = false;
    player->pinGForce = false;
    player->pinThermometer = false;
    
    // Reset Garage
    for(int i=0; i<10; i++) {
        player->ownedCars[i] = false;
        player->ownedUpgrades[i] = false; // [NEW]
    }
    
    player->ownedCars[1] = true; 
    player->currentCarIndex = 1;
    player->isDrivingUpgrade = false; // [NEW]

    // --- Progression ---
    player->tutorialFinished = false; // Restart tutorial

    // 4. RESET PHONE
    phone->isOpen = false;
    phone->slideAnim = 0.0f;
    phone->currentApp = APP_HOME;
    phone->activeTaskCount = 0;
    
    // Clear Tasks
    for(int i=0; i<5; i++) {
        phone->tasks[i].status = JOB_AVAILABLE;
        phone->tasks[i].timeLimit = 0;
    }

    // Reset Settings
    phone->settings.masterVolume = 1.0f;
    phone->settings.sfxVolume = 1.0f;
    phone->settings.mute = false;

    // Reset Music
    phone->music.isPlaying = false;
    phone->music.currentSongIdx = 0;

    TraceLog(LOG_INFO, "SAVE: Save file deleted and game state manually reset.");
}
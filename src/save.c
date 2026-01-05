#include "save.h"
#include <stdio.h>
#include <string.h> // for memset

// Simple XOR Encryption Key
const unsigned char KEY = 0xAA; // 10101010

void Obfuscate(unsigned char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= KEY;
    }
}

bool SaveGame(Player *player, PhoneState *phone) {
    GameSaveData data = {0};
    
    // 1. PACK DATA (Copy from Game Objects to Save Struct)
    data.version = SAVE_VERSION;
    
    // Player Physics/Pos
    data.position = player->position;
    data.angle = player->angle;
    data.fuel = player->fuel;
    
    // Player Economy
    data.money = player->money;
    data.totalDeliveries = player->totalDeliveries;
    data.totalEarnings = player->totalEarnings;
    data.transactionCount = player->transactionCount;
    
    // Copy the history array safely
    for(int i=0; i<MAX_TRANSACTIONS; i++) {
        data.history[i] = player->history[i];
    }

    // Player Upgrades
    data.hasCarMonitorApp = player->hasCarMonitorApp;
    data.unlockGForce = player->unlockGForce;
    data.unlockThermometer = player->unlockThermometer;
    
    // Player UI Pins
    data.pinSpeed = player->pinSpeed;
    data.pinFuel = player->pinFuel;
    data.pinAccel = player->pinAccel;
    data.pinGForce = player->pinGForce;
    data.pinThermometer = player->pinThermometer;

    // Phone Settings
    data.settings = phone->settings;

    // 2. ENCRYPT
    Obfuscate((unsigned char*)&data, sizeof(GameSaveData));

    // 3. WRITE TO FILE
    FILE *file = fopen(SAVE_FILE_NAME, "wb");
    if (!file) {
        TraceLog(LOG_ERROR, "SAVE: Could not open file for writing.");
        return false;
    }

    size_t written = fwrite(&data, sizeof(GameSaveData), 1, file);
    fclose(file);

    if (written == 1) {
        TraceLog(LOG_INFO, "SAVE: Game saved successfully.");
        // Optional: Trigger a phone notification here if you had access to the function
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

    if (read != 1) {
        TraceLog(LOG_WARNING, "SAVE: File empty or corrupted.");
        return false;
    }

    // 1. DECRYPT
    Obfuscate((unsigned char*)&data, sizeof(GameSaveData));

    // 2. VERSION CHECK
    if (data.version != SAVE_VERSION) {
        TraceLog(LOG_WARNING, "SAVE: Version mismatch. Starting fresh to prevent crash.");
        return false;
    }

    // 3. UNPACK DATA (Copy from Save Struct to Game Objects)
    
    // Physics
    player->position = data.position;
    player->angle = data.angle;
    player->fuel = data.fuel;
    
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

    // Settings
    phone->settings = data.settings;

    TraceLog(LOG_INFO, "SAVE: Game Loaded Successfully.");
    return true;
}

void ResetSaveGame(Player *player, PhoneState *phone) {
    // 1. Delete the file
    remove(SAVE_FILE_NAME);
    
    // 2. Reset Player Stats (Keep position or reset it? Let's reset money/stats)
    player->money = 0;
    player->fuel = MAX_FUEL;
    player->totalDeliveries = 0;
    player->transactionCount = 0;
    player->hasCarMonitorApp = false;
    
    // Note: We usually DON'T reset Settings (Volume) as that's annoying for users.
    // But if you want a hard factory reset:
    phone->settings.masterVolume = 0.8f;
    phone->settings.mute = false;
    
    TraceLog(LOG_INFO, "SAVE: Save file deleted and state reset.");
}
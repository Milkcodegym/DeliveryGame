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

#ifndef PHONE_H
#define PHONE_H

#include "raylib.h"
#include <stdbool.h> 
#include "save.h"
#include "player.h" 
#include "map.h"
#include "maps_app.h" 
#include "delivery_app.h"
#include "car_monitor.h"

// --- Forward Declarations ---
// We don't need typedef Player Player here if we include player.h
struct GameMap; 

// --- Constants ---
#define PHONE_WIDTH 320
#define PHONE_HEIGHT 640
#define SCREEN_WIDTH 280
#define SCREEN_HEIGHT 560
#define SCREEN_OFFSET_X 20
#define SCREEN_OFFSET_Y 40

// --- App States ---
// Renamed to PhoneApp to match phone.c logic
typedef enum {
    APP_HOME,
    APP_DELIVERY,
    APP_MAP,
    APP_BANK,
    APP_MUSIC,
    APP_SETTINGS,
    APP_BROWSER,
    APP_CAR_MONITOR
} PhoneApp;

// --- Delivery Data ---
typedef enum {
    JOB_AVAILABLE,
    JOB_ACCEPTED,
    JOB_PICKED_UP,
    JOB_DELIVERED,
} JobStatus;

typedef struct DeliveryTask {
    char restaurant[32];
    Vector2 restaurantPos; 
    
    char customer[32];
    Vector2 customerPos;   
    
    float pay;
    float maxPay;
    float distance;        
    JobStatus status; 
    int jobType;           
    float fragility;       
    bool isHeavy;          
    float timeLimit;       
    double creationTime;   
    double refreshTimer;   
    char description[64];  
} DeliveryTask;

// --- Music Data ---
typedef struct Song {
    char title[32];
    char artist[32];
    char filePath[64];
    Music stream;     
    float duration;    
} Song;

// Renamed to MusicPlayer to match phone.c logic
typedef struct MusicPlayer {
    bool isPlaying;
    int currentSongIdx;
    Song library[3]; 
} MusicPlayer;

// --- Settings Data ---
// Renamed to PhoneSettings to match save.h logic
typedef struct PhoneSettings {
    float masterVolume;
    float sfxVolume;
    bool mute;
} PhoneSettings;

// --- Main Phone State ---
typedef struct PhoneState {
    // Visuals
    RenderTexture2D screenTexture;
    float slideAnim; 
    bool isOpen;
    int activeTaskCount;
    // Navigation
    PhoneApp currentApp; // Updated type name
    
    // App Specific Data
    DeliveryTask tasks[5];
    MusicPlayer music;      // Updated type name
    PhoneSettings settings; // Updated type name
} PhoneState;

// Functions
void InitPhone(PhoneState *phone, struct GameMap *map); 
void UpdatePhone(PhoneState *phone, Player *player, struct GameMap *map); 
void DrawPhone(PhoneState *phone, Player *player, struct GameMap *map, Vector2 mouse, bool click);
void UnloadPhone(PhoneState *phone);
// Added notification helper to header so other files can use it
void ShowPhoneNotification(const char *text, Color color); 
// Add this to the bottom of phone.h
void ShowTutorialHelp(void); // From tutorial.c
#endif
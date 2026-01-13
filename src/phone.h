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

// --- Forward Declarations ---
// [FIX] We use forward declarations instead of #includes to prevent 
// "Circular Dependency" errors (where phone needs player, and player needs phone).
typedef struct Player Player;
typedef struct GameMap GameMap;

// --- Constants ---
#define PHONE_WIDTH 320
#define PHONE_HEIGHT 640
#define SCREEN_WIDTH 280
#define SCREEN_HEIGHT 560
#define SCREEN_OFFSET_X 20
#define SCREEN_OFFSET_Y 40

// --- App States ---
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
#define MAX_SONGS 64 

typedef struct {
    char title[64];
    char artist[64];
    char filePath[128];
    Music stream;
    float duration;
} Song;

typedef struct {
    Song library[MAX_SONGS];
    int songCount;      
    int currentSongIdx;
    bool isPlaying;
    bool isInitialized; 
} MusicApp;

// --- Settings Data ---
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
    PhoneApp currentApp; 
    
    // App Specific Data
    DeliveryTask tasks[5];
    MusicApp music;      
    PhoneSettings settings; 
} PhoneState;

// --- Functions ---
void InitPhone(PhoneState *phone, GameMap *map); 
void UpdatePhone(PhoneState *phone, Player *player, GameMap *map); 
void DrawPhone(PhoneState *phone, Player *player, GameMap *map, Vector2 mouse, bool click);
void UnloadPhone(PhoneState *phone);

void ShowPhoneNotification(const char *text, Color color); 
void ShowTutorialHelp(void); 

#endif // PHONE_H
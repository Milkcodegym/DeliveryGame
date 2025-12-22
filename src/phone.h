#ifndef PHONE_UI_H
#define PHONE_UI_H

#include "raylib.h"
#include <stdbool.h> 

// --- Forward Declarations ---
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
    APP_BROWSER
} AppState;

// --- Delivery Data ---
typedef enum {
    JOB_AVAILABLE,
    JOB_ACCEPTED,
    JOB_PICKED_UP,
    JOB_DELIVERED
} JobStatus;

typedef struct DeliveryTask {
    char restaurant[32];
    Vector2 restaurantPos; // Coordinates of the store
    
    char customer[32];
    Vector2 customerPos;   // Coordinates of the house
    
    float pay;
    float distance;        // Estimated distance
    JobStatus status;      // Current state of the job
} DeliveryTask;

// --- Music Data ---
typedef struct Song {
    char title[32];
    char artist[32];
    char filePath[64];
    Music stream;     
    float duration;    
} Song;

typedef struct MusicState {
    bool isPlaying;
    int currentSongIdx;
    Song library[3]; 
} MusicState;

// --- Settings Data ---
typedef struct SettingsState {
    float masterVolume;
    float sfxVolume;
    bool mute;
} SettingsState;

// --- Main Phone State ---
typedef struct PhoneState {
    // Visuals
    RenderTexture2D screenTexture;
    Texture2D themeAtlas; 
    float slideAnim; 
    bool isOpen;
    
    // Navigation
    AppState currentApp;
    
    // App Specific Data
    DeliveryTask tasks[5];
    MusicState music;     
    SettingsState settings;
} PhoneState;

void InitPhone(PhoneState *phone, GameMap *map); // UPDATED: Needs Map to generate jobs
void UpdatePhone(PhoneState *phone, Player *player, GameMap *map); 
void DrawPhone(PhoneState *phone, Player *player, GameMap *map);
void UnloadPhone(PhoneState *phone);

#endif
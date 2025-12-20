#ifndef PHONE_UI_H
#define PHONE_UI_H

#include "raylib.h"
#include <stdbool.h> 

// --- Forward Declarations ---
// We tell the compiler these structs exist, so we can use pointers to them.
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

// --- Data Structures ---
typedef struct DeliveryTask {
    char restaurant[32];
    char customer[32];
    float pay;
    float distance;
    bool active;
} DeliveryTask;

// Music Data
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

// Settings Data
typedef struct SettingsState {
    float masterVolume;
    float sfxVolume;
    bool mute;
} SettingsState;

typedef struct PhoneState {
    // Visuals
    RenderTexture2D screenTexture;
    Texture2D themeAtlas; // Added for the custom UI texture we discussed
    float slideAnim; 
    bool isOpen;
    
    // Navigation
    AppState currentApp;
    
    // App Specific Data
    // NOTE: walletBalance removed (It is now inside Player)
    DeliveryTask tasks[5];
    MusicState music;     
    SettingsState settings;
} PhoneState;

void InitPhone(PhoneState *phone);

// Updated: Now accepts Player/Map to update GPS logic
void UpdatePhone(PhoneState *phone, Player *player, GameMap *map); 

// Updated: Now accepts Player/Map to draw Bank Balance and GPS
void DrawPhone(PhoneState *phone, Player *player, GameMap *map);

void UnloadPhone(PhoneState *phone);

#endif
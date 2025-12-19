#ifndef PHONE_H
#define PHONE_H

#include "raylib.h"

// --- Dimensions ---
#define PHONE_WIDTH 300
#define PHONE_HEIGHT 600
#define PHONE_SCREEN_WIDTH 260
#define PHONE_SCREEN_HEIGHT 540

// --- App States ---
typedef enum {
    APP_HOME,       // Main Grid of Icons
    APP_MAP,
    APP_DELIVERY,
    APP_BROWSER,
    APP_RADIO
} PhoneApp;

typedef struct Phone {
    // Animation State
    bool active;           // Is the phone requested to be up?
    float animProgress;    // 0.0f (Hidden) -> 1.0f (Fully Visible)
    
    // Internal State
    PhoneApp currentApp;
    RenderTexture2D target; // The "Virtual Screen" texture
    
    // Home Screen Selection
    int selectedIconIndex; 
} Phone;

void InitPhone(Phone *phone);
void UpdatePhone(Phone *phone, float dt);
void DrawPhone(Phone *phone);
void UnloadPhone(Phone *phone);

#endif
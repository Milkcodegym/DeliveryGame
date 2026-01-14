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

#include "tutorial.h"
#include "raylib.h"
#include "raymath.h" 
#include <stdio.h>
#include <string.h>

// --- TUTORIAL STATES ---
typedef enum {
    TUT_INACTIVE,
    TUT_WELCOME,          
    TUT_PHONE_INTRO,      
    TUT_PHONE_APPS,      
    TUT_CONTROLS,
    TUT_CRASH_INTRO,
    TUT_SPAWN_FIRST_JOB,  
    TUT_WAIT_JOB,         
    TUT_FIRST_DELIVERY,   
    TUT_SECOND_INTRO,     
    TUT_SECOND_DELIVERY,  
    TUT_EVENT_INTRO,      
    TUT_REFUEL_INTRO,     
    TUT_REFUEL_ACTION,    
    TUT_MECH_INTRO,       
    TUT_MECH_ACTION,      
    TUT_DEALER_INTRO,     
    TUT_DEALER_ACTION, 
    TUT_OUTRO,   
    TUT_FINISHED          
} TutState;


// --- LOCALS ---
static TutState currentState = TUT_INACTIVE;
static float stateTimer = 0.0f;
static int currentAppTab = 0; 
static bool hasEnteredDealership = false;
static bool showingHelp = false; 
static int eventStep = 0;
static bool check;

// --- INTERNAL HELPERS ---

// Forces a specific job type for the tutorial into Slot 0
static void ForceSpawnJob(PhoneState *phone, GameMap *map, bool isFragile) {
    if (!map) {
        TraceLog(LOG_WARNING, "TUTORIAL: Attempted to spawn job with NULL map!");
        return; 
    }

    int houseIdx = -1;
    int storeIdx = -1;
    
    // Find ANY valid store (Food, Cafe, Market, etc.)
    for(int i=0; i<map->locationCount; i++) {
        if(map->locations[i].type == LOC_HOUSE && houseIdx == -1) houseIdx = i;
        
        int t = map->locations[i].type;
        if(t >= 1 && t <= 6 && storeIdx == -1) storeIdx = i;
    }

    if (houseIdx != -1 && storeIdx != -1) {
        // Clear slot 0
        memset(&phone->tasks[0], 0, sizeof(DeliveryTask));
        
        phone->tasks[0].status = JOB_AVAILABLE;
        snprintf(phone->tasks[0].restaurant, 32, "%s", map->locations[storeIdx].name);
        phone->tasks[0].restaurantPos = map->locations[storeIdx].position;
        snprintf(phone->tasks[0].customer, 32, "Tutorial House");
        phone->tasks[0].customerPos = map->locations[houseIdx].position;
        phone->tasks[0].pay = 150.0f; 
        phone->tasks[0].maxPay = 150.0f; 
        phone->tasks[0].distance = Vector2Distance(phone->tasks[0].restaurantPos, phone->tasks[0].customerPos);
        phone->tasks[0].creationTime = GetTime();
        
        // Reset Constraints
        phone->tasks[0].fragility = 0.0f;
        phone->tasks[0].timeLimit = 0;
        phone->tasks[0].isHeavy = false;

        if (isFragile) {
            snprintf(phone->tasks[0].description, 64, "Fragile Glass - Careful!");
            phone->tasks[0].fragility = 0.8f; 
        } else {
            snprintf(phone->tasks[0].description, 64, "First Day - Standard Run");
        }
        TraceLog(LOG_INFO, "TUTORIAL: Job Spawned. Store: %s", phone->tasks[0].restaurant);
    } else {
        TraceLog(LOG_WARNING, "TUTORIAL: Could not find valid locations for job!");
    }
}

// Draw multi-line text centered horizontally
static void DrawCenteredTextMulti(const char* text, float centerX, float startY, int fontSize, Color color) {
    char buffer[1024];
    strncpy(buffer, text, 1024);
    
    char lineBuffer[256];
    int lineIndex = 0;
    float y = startY;

    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            lineBuffer[lineIndex] = '\0';
            int w = MeasureText(lineBuffer, fontSize);
            DrawText(lineBuffer, centerX - w/2, y, fontSize, color);
            y += fontSize + 5;
            lineIndex = 0;
        } else {
            lineBuffer[lineIndex++] = text[i];
        }
    }
    if (lineIndex > 0) {
        lineBuffer[lineIndex] = '\0';
        int w = MeasureText(lineBuffer, fontSize);
        DrawText(lineBuffer, centerX - w/2, y, fontSize, color);
    }
}

static void DrawFakeAppScreen(int appIndex, float x, float y, float w, float h, float scale) {
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    int titleSize = (int)(20 * scale);
    int bodySize = (int)(12 * scale);

    switch(appIndex) {
        case 0: // JOBS
            DrawRectangle(x, y, w, 40*scale, ORANGE);
            DrawText("JOBS", x + 10*scale, y + 10*scale, titleSize, WHITE);
            for(int i=0; i<3; i++) {
                float rowY = y + (50*scale) + (i*50*scale);
                DrawRectangle(x + 10*scale, rowY, w - 20*scale, 40*scale, LIGHTGRAY);
                DrawText(i==0?"Pizza Delivery":"Package Run", x + 20*scale, rowY + 5*scale, bodySize, BLACK);
                DrawText(i==0?"Hot - Rush!":"Standard", x + 20*scale, rowY + 20*scale, bodySize, GRAY);
                DrawText("$25", x + w - 40*scale, rowY + 10*scale, bodySize, GREEN);
            }
            break;
            
        case 1: // MAPS
            DrawRectangle(x, y, w, h, (Color){220, 220, 220, 255}); 
            for(int i=0; i<5; i++) DrawLine(x + (i*40*scale), y, x + (i*40*scale), y+h, WHITE);
            for(int i=0; i<8; i++) DrawLine(x, y + (i*40*scale), x+w, y + (i*40*scale), WHITE);
            
            DrawCircle(x+w/2, y+h/2, 6*scale, BLUE); 
            DrawCircleLines(x+w/2, y+h/2, 20*scale, Fade(BLUE, 0.3f)); 
            
            DrawLineEx((Vector2){x+w/2, y+h/2}, (Vector2){x+w-20, y+40}, 3.0f, RED);
            DrawCircle(x+w-20, y+40, 5*scale, RED); 
            
            DrawRectangle(x, y + h - 40*scale, w, 40*scale, Fade(WHITE, 0.9f));
            DrawText("1.2km to Target", x + 10*scale, y + h - 30*scale, bodySize, BLACK);
            
            DrawRectangle(x + w - 35*scale, y + h - 35*scale, 30*scale, 30*scale, BLUE);
            DrawText("O", x + w - 25*scale, y + h - 28*scale, bodySize, WHITE);
            break;
            
        case 2: // BANK
            DrawRectangle(x, y, w, 70*scale, DARKGREEN);
            DrawText("BALANCE", x + 20*scale, y + 10*scale, bodySize, LIGHTGRAY);
            DrawText("$ 50.00", x + 20*scale, y + 30*scale, (int)(24*scale), WHITE);
            DrawText("Recent Activity", x + 10*scale, y + 80*scale, bodySize, DARKGRAY);
            DrawLine(x+10*scale, y+95*scale, x+w-10*scale, y+95*scale, LIGHTGRAY);
            DrawText("Hospital Bill", x + 10*scale, y + 110*scale, bodySize, BLACK);
            DrawText("-$200.00", x + w - 70*scale, y + 110*scale, bodySize, RED);
            break;
            
        case 3: // MUSIC
            DrawRectangle(x, y, w, h, (Color){30, 0, 40, 255}); 
            DrawRectangle(x + w/2 - 40*scale, y + 60*scale, 80*scale, 80*scale, PURPLE);
            DrawText("Neon Drive", x + w/2 - 35*scale, y + 160*scale, titleSize, WHITE);
            DrawText("|<   ||   >|", x + w/2 - 40*scale, y + 250*scale, titleSize, WHITE);
            DrawRectangle(x + 20*scale, y + 220*scale, w - 40*scale, 4*scale, GRAY);
            DrawRectangle(x + 20*scale, y + 220*scale, (w - 40*scale)*0.6f, 4*scale, GREEN);
            break;
            
        case 4: // SETTINGS
            DrawRectangle(x, y, w, 40*scale, LIGHTGRAY);
            DrawText("Settings", x + 20*scale, y + 10*scale, titleSize, BLACK);
            float sy = y + 60*scale;
            DrawText("Master Volume", x + 20*scale, sy, bodySize, BLACK);
            DrawRectangle(x + 20*scale, sy + 20*scale, w - 40*scale, 6*scale, GRAY);
            DrawCircle(x + 20*scale + (w-40*scale)*0.8f, sy + 23*scale, 8*scale, BLUE);
            break;
        case 5: // CAR MONITOR
            DrawRectangle(x, y, w, h, (Color){20, 20, 25, 255}); // Dark Background
            DrawText("MyCarMonitor", x + 20*scale, y + 20*scale, titleSize, SKYBLUE);
            DrawLine(x + 20*scale, y + 50*scale, x + w - 20*scale, y + 50*scale, DARKGRAY);
            
            // Draw Fake Toggles
            const char* fakeToggles[] = { "Speedometer", "Fuel Gauge", "G-Force" };
            for(int i=0; i<3; i++) {
                float rowY = y + 70*scale + (i*50*scale);
                // Draw toggle box
                DrawRectangle(x + 20*scale, rowY, w - 40*scale, 40*scale, (i==0) ? GREEN : Fade(DARKGRAY, 0.5f));
                DrawRectangleLines(x + 20*scale, rowY, w - 40*scale, 40*scale, BLACK);
                DrawText(fakeToggles[i], x + 30*scale, rowY + 12*scale, bodySize, WHITE);
                // Draw toggle circle
                DrawCircle(x + w - 40*scale, rowY + 20*scale, 6*scale, WHITE);
            }
            
            // Fake Stats at bottom
            DrawText("Top Speed: 180 km/h", x + 20*scale, y + 250*scale, bodySize, WHITE);
            DrawText("0-100: 4.2 s", x + 20*scale, y + 270*scale, bodySize, WHITE);
            break;
    }
}

// Generic Window Helper - Returns true if "Next" button is clicked
static bool DrawTutWindow(const char* title, const char* body, bool showNext) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float scale = (float)sh / 720.0f;
    if (scale < 0.6f) scale = 0.6f;

    float w = 600 * scale; 
    float h = 350 * scale;
    float x = (sw - w) / 2;
    float y = (sh - h) / 2;

    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f)); 
    DrawRectangle(x, y, w, h, RAYWHITE);
    DrawRectangleLines(x, y, w, h, BLACK);
    
    DrawRectangle(x, y, w, 50 * scale, DARKBLUE);
    
    int titleSize = (int)(24 * scale);
    int tw = MeasureText(title, titleSize);
    DrawText(title, x + (w - tw)/2, y + 12*scale, titleSize, WHITE);
    
    int bodySize = (int)(18 * scale);
    DrawCenteredTextMulti(body, x + w/2, y + 80*scale, bodySize, DARKGRAY);

    if (showNext) {
        Rectangle btnNext = { x + w - 120*scale, y + h - 60*scale, 100*scale, 40*scale };
        bool hover = CheckCollisionPointRec(GetMousePosition(), btnNext);
        DrawRectangleRec(btnNext, hover ? BLUE : DARKBLUE);
        DrawText("NEXT >", btnNext.x + 20*scale, btnNext.y + 10*scale, (int)(20*scale), WHITE);
        
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return true;
    }
    return false;
}

// Reusable App Guide Screen
static void DrawAppGuideScreen(int sw, int sh, float scale, Vector2 mouse, bool click, bool isHelpMode) {
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));
    float w = 800 * scale, h = 500 * scale;
    float x = (sw-w)/2, y = (sh-h)/2;
    DrawRectangle(x, y, w, h, RAYWHITE);
    
    float sidebarW = 200 * scale;
    DrawRectangle(x, y, sidebarW, h, LIGHTGRAY);
    const char* tabs[] = { "JOBS", "MAPS", "BANK", "MUSIC", "SETTINGS", "MONITOR" };
    
    for (int i=0; i<6; i++) {
        Rectangle tabRect = { x, y + 50*scale + (i*60*scale), sidebarW, 50*scale };
        bool selected = (currentAppTab == i);
        DrawRectangleRec(tabRect, selected ? WHITE : LIGHTGRAY);
        DrawText(tabs[i], tabRect.x + 20*scale, tabRect.y + 15*scale, (int)(20*scale), selected ? BLUE : DARKGRAY);
        if (CheckCollisionPointRec(mouse, tabRect) && click) currentAppTab = i;
    }

    float cx = x + sidebarW + 20*scale;
    float cy = y + 20*scale;
    DrawText(isHelpMode ? "PHONE HELP" : "APP GUIDE", cx, cy, (int)(30*scale), BLACK);
    
    DrawFakeAppScreen(currentAppTab, cx, cy + 60*scale, 200*scale, 350*scale, scale);
    
    const char* expl = "";
    if (currentAppTab == 0) expl = "JOBS (Key: 1)\n\nAccept delivery contracts here.\nPay attention to Pay, Distance,\nand Constraints (Fragile/Heavy).";
    if (currentAppTab == 1) expl = "MAPS (Key: 2)\n\nLive GPS Navigation.\nUse the Black Button to re-center.\nFind Gas Stations (Pump Icon),\nMechanics (Wrench Icon)\nand Dealerships (Car Icon).\nDouble-click your destination \nto find the shortest route.";
    if (currentAppTab == 2) expl = "BANK (Key: 3)\n\nTrack your financial health.\nView income and debts.\n(You start with debt).";
    if (currentAppTab == 3) expl = "MUSIC (Key: 4)\n\nPlay your own MP3/OGG files\nor use the built-in radio.\nTo insert files, just put them\nin the resources/music folder\nand restart the game!";
    if (currentAppTab == 4) expl = "SETTINGS (Key: 5)\n\nAdjust Volume levels.\nReset Save Data if stuck.\nAccess this help menu.";
    if (currentAppTab == 5) expl = "CAR MONITOR (Key: 6)\n\nUse it to PIN extra gauges\nto your screen (G-Force, Temp).\nAlso shows vehicle stats like\n0-100 times and Fuel Range.";

    DrawText(expl, cx + 250*scale, cy + 60*scale, (int)(18*scale), DARKGRAY);

    Rectangle btnDone = { x + w - 180*scale, y + h - 60*scale, 160*scale, 40*scale };
    DrawRectangleRec(btnDone, GREEN); 
    const char* btnTxt = isHelpMode ? "CLOSE HELP" : "LET'S DRIVE";
    DrawText(btnTxt, btnDone.x + 25*scale, btnDone.y + 10*scale, (int)(16*scale), BLACK);
    
    if (CheckCollisionPointRec(mouse, btnDone) && click) {
        if (isHelpMode) showingHelp = false;
        else currentState = TUT_CONTROLS;
    }
}

// --- PUBLIC FUNCTIONS ---

void InitTutorial() {
    stateTimer = 0.0f;
    hasEnteredDealership = false;
    showingHelp = false;
    eventStep = 0;
    check=0;
    
}

void SkipTutorial(Player *player, PhoneState *phone) {
    player->tutorialFinished = true;
    currentState = TUT_FINISHED;
    SaveGame(player, phone);
}

void ShowTutorialHelp() {
    showingHelp = true;
    currentAppTab = 0; 
}

bool UpdateTutorial(Player *player, PhoneState *phone, GameMap *map, float dt, bool isRefueling, bool isMechanicOpen) {
    if (showingHelp) return true; 

    player->fuelConsumption = 0.0f;

    if (player->tutorialFinished) return false;
    if (currentState == TUT_INACTIVE) currentState = TUT_WELCOME;

    stateTimer += dt;
    bool blocking = false;

    switch (currentState) {
        case TUT_WELCOME:
        case TUT_PHONE_INTRO:
        case TUT_PHONE_APPS:
        case TUT_CRASH_INTRO:
        case TUT_SECOND_INTRO:
        case TUT_EVENT_INTRO:
        case TUT_REFUEL_INTRO:
        case TUT_MECH_INTRO:
        case TUT_DEALER_INTRO:
            blocking = true;
            break;

        case TUT_CONTROLS:
            if (player->current_speed > 5.0f) {
                stateTimer = 0.0f;
                currentState =TUT_CRASH_INTRO; // To Crash Warning
            }
            break;
        case TUT_SPAWN_FIRST_JOB:
            ForceSpawnJob(phone, map, false); 
            ShowPhoneNotification("NEW JOB AVAILABLE!", ORANGE);
            currentState = TUT_WAIT_JOB;
            break;

        case TUT_WAIT_JOB:
            if (phone->tasks[0].status == JOB_ACCEPTED) currentState = TUT_FIRST_DELIVERY;
            break;

        case TUT_FIRST_DELIVERY:
            if (phone->tasks[0].status == JOB_DELIVERED) currentState = TUT_SECOND_INTRO;
            break;

        case TUT_SECOND_DELIVERY:
            if (phone->tasks[0].status != JOB_ACCEPTED && phone->tasks[0].status != JOB_PICKED_UP) {
                 if (strstr(phone->tasks[0].description, "Fragile") == NULL) {
                      ForceSpawnJob(phone, map, true); 
                      ShowPhoneNotification("FRAGILE JOB RECEIVED!", RED);
                 }
            }
            if (phone->tasks[0].status == JOB_DELIVERED) {
                currentState = TUT_EVENT_INTRO;
                Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
                TriggerSpecificEvent(map, EVENT_ROADWORK, player->position, fwd);
            }
            break;

        case TUT_REFUEL_ACTION:
            if (player->fuel >= (player->maxFuel * 0.9f) && !isRefueling) {
                 currentState = TUT_MECH_INTRO;
            }
            break;

        case TUT_MECH_ACTION:
            
            if (isMechanicOpen) check=1;
            if (!isMechanicOpen && check) currentState = TUT_DEALER_INTRO;
            break;

        case TUT_DEALER_ACTION:
            if (GetDealershipState() == DEALERSHIP_ACTIVE) hasEnteredDealership = true;
            if (hasEnteredDealership && GetDealershipState() == DEALERSHIP_INACTIVE) {
                currentState = TUT_OUTRO;
            }
            break;

        case TUT_OUTRO:
            blocking = true;
            break;
            
        default: break;
    }

    return blocking;
}

void DrawTutorial(Player *player, PhoneState *phone, bool isRefueling) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    float scale = (float)sh / 720.0f;
    if (scale < 0.6f) scale = 0.6f;

    // [FIX] Define a consistent Y position for instructions at the BOTTOM of screen
    // This puts the text safely below the Speedometer/Center HUD
    int instructionY = sh - (int)(150 * scale);
    int subInstructionY = sh - (int)(110 * scale);

    Vector2 mouse = GetMousePosition();
    bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (showingHelp) {
        DrawAppGuideScreen(sw, sh, scale, mouse, click, true);
        return;
    }

    if (player->tutorialFinished || currentState == TUT_INACTIVE) return;

    switch (currentState) {
        case TUT_WELCOME: {
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
            float w = 500 * scale, h = 300 * scale;
            float x = (sw-w)/2, y = (sh-h)/2;
            DrawRectangle(x, y, w, h, RAYWHITE);
            DrawRectangleLines(x, y, w, h, BLACK);
 // Lock fuel consumption during tutorial
            int titleS = (int)(28*scale);
            int bodyS = (int)(18*scale);
            DrawText("WELCOME TO RAY-CITY", x + (w-MeasureText("WELCOME TO RAY-CITY", titleS))/2, y + 30*scale, titleS, DARKBLUE);
            DrawCenteredTextMulti("You're new here. You're broke. You have bills.\nReady to start your delivery career?", x + w/2, y + 80*scale, bodyS, DARKGRAY);
            DrawCenteredTextMulti("WARNING: PROGRESS IS NOT SAVED\nDURING THE TUTORIAL!", x + w/2, y + 160*scale, (int)(16*scale), RED);

            Rectangle btnYes = { x + 50*scale, y + 220*scale, 150*scale, 50*scale };
            Rectangle btnNo = { x + 300*scale, y + 220*scale, 150*scale, 50*scale };
            DrawRectangleRec(btnYes, GREEN);
            DrawText("START TUTORIAL", btnYes.x + 10*scale, btnYes.y + 15*scale, (int)(16*scale), BLACK);
            DrawRectangleRec(btnNo, LIGHTGRAY);
            DrawText("SKIP", btnNo.x + 50*scale, btnNo.y + 15*scale, (int)(16*scale), BLACK);

            if (CheckCollisionPointRec(mouse, btnYes) && click) currentState = TUT_PHONE_INTRO;
            if (CheckCollisionPointRec(mouse, btnNo) && click) SkipTutorial(player, phone);
            break;
        }

        case TUT_PHONE_INTRO: {
            // Intro
            int w = 600 * scale, h = 350 * scale;
            int x = (sw-w)/2, y = (sh-h)/2;
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
            DrawRectangle(x, y, w, h, RAYWHITE);
            DrawRectangleLines(x, y, w, h, BLACK);
            DrawRectangle(x, y, w, 50*scale, DARKBLUE);
            
            DrawText("YOUR TOOLS", x + 20*scale, y + 15*scale, (int)(24*scale), WHITE);
            DrawCenteredTextMulti("This phone is your lifeline.\n\nUse it to accept jobs, navigate,\nand manage finances.\n\nApps are accessed via Shortcuts (1-6).", x + w/2 - 80*scale, y + 80*scale, (int)(18*scale), DARKGRAY);

            float px = x + w - 160*scale; float py = y + 60*scale;
            DrawRectangle(px, py, 130*scale, 250*scale, BLACK);
            DrawRectangle(px + 5*scale, py + 10*scale, 120*scale, 230*scale, RAYWHITE);
            DrawText("RayOS", px + 40*scale, py + 100*scale, (int)(16*scale), LIGHTGRAY);

            Rectangle btnNext = { x + w - 120*scale, y + h - 60*scale, 100*scale, 40*scale };
            bool hover = CheckCollisionPointRec(mouse, btnNext);
            DrawRectangleRec(btnNext, hover ? BLUE : DARKBLUE);
            DrawText("NEXT >", btnNext.x + 20*scale, btnNext.y + 10*scale, (int)(20*scale), WHITE);
            if (hover && click) currentState = TUT_PHONE_APPS;
            break;
        }

        case TUT_PHONE_APPS:
            DrawAppGuideScreen(sw, sh, scale, mouse, click, false);
            break;

        case TUT_CONTROLS:
            // This was already at bottom, kept it safe
            DrawText("USE [W][A][S][D] TO DRIVE", sw/2 - 150*scale, sh - 150*scale, (int)(30*scale), WHITE);
            DrawText("Reach 25 KMH to continue", sw/2 - 120*scale, sh - 110*scale, (int)(20*scale), LIGHTGRAY);
            break;

        case TUT_CRASH_INTRO: 
            if (DrawTutWindow("SAFETY WARNING", 
                "CRASHING COSTS MONEY.\n\nIf you hit walls or cars, you lose HEALTH (Top Right).\nIf Health hits 0, you pay heavy bills\nand respawn at a Mechanic.\n\nDrive carefully.", true)) {
                currentState = TUT_SPAWN_FIRST_JOB;
            }
            break;

        case TUT_WAIT_JOB:
            // [FIX] Moved to Bottom
            DrawText("OPEN PHONE [TAB]", sw/2 - 100*scale, instructionY, (int)(20*scale), YELLOW);
            DrawText("ACCEPT THE JOB IN 'JOBS' APP", sw/2 - 160*scale, subInstructionY, (int)(20*scale), YELLOW);
            break;

        case TUT_FIRST_DELIVERY:
            // [FIX] Moved to Bottom
            DrawText("FOLLOW THE RED GPS LINE", sw/2 - 150*scale, instructionY, (int)(24*scale), RED);
            break;

        case TUT_SECOND_INTRO:
            if (DrawTutWindow("NOT BAD, ROOKIE", "You handled that box nicely.\n\nNow let's try a REAL job.\nThis cargo is FRAGILE (Glassware).\nIf you crash or turn too hard, you lose money.\n\nWatch the Cargo Integrity Meter on your HUD.", true)) {
                currentState = TUT_SECOND_DELIVERY;
            }
            break;

        case TUT_SECOND_DELIVERY:
            // [FIX] Moved to Bottom
            if (phone->tasks[0].status == JOB_AVAILABLE) 
                DrawText("OPEN PHONE AND ACCEPT FRAGILE JOB", sw/2 - 200*scale, instructionY, (int)(20*scale), ORANGE);
            else 
                DrawText("DRIVE CAREFULLY - DON'T BREAK IT", sw/2 - 180*scale, instructionY, (int)(24*scale), ORANGE);
            break;

        case TUT_EVENT_INTRO:
            if (eventStep == 0) {
                // Show First Window
                if (DrawTutWindow("DELIVERY TYPES", "Being a delivery driver is challenging.\n\nThere are many delivery cargo types \nthat have different requirements.\n\n\nFragile, heavy, hot,\nyou must be careful with these cargo types.", true)) {
                    eventStep = 1; // Move to next step permanently
                }
            } 
            else {
                // Show Second Window
                if (DrawTutWindow("ROAD EVENTS", "The city is unpredictable.\n\nAccidents, Roadworks, and Police stops\ncan block your path.\n\nIf you see cones or signs, slow down\nor find another route.", true)) {
                    currentState = TUT_REFUEL_INTRO;
                }
            }
            break;

        case TUT_REFUEL_INTRO:
            if (DrawTutWindow("RUNNING ON FUMES", "Your fuel gauge is low.\nYou can't deliver if you can't drive.\n\nFind the nearest GAS STATION.\nDrive to the pumps and press [E].", true)) {
                // --- CHANGE HERE: Drain fuel manually ---
                player->fuel = 1.0f; 
                currentState = TUT_REFUEL_ACTION; 
                // ----------------------------------------
            }
            break;

        case TUT_REFUEL_ACTION:
            // --- CHANGE HERE: Dynamic Instructions ---
            if (isRefueling) {
                DrawText("BUY FUEL UNTIL TANK IS FULL", sw/2 - 150*scale, instructionY, (int)(20*scale), GREEN);
            } 
            else if (player->fuel < player->maxFuel * 0.9f) {
                DrawText("GO TO GAS STATION & FILL TANK", sw/2 - 160*scale, instructionY, (int)(20*scale), YELLOW);
            }
            else {
                DrawText("TANK FULL! CLOSING MENU...", sw/2 - 140*scale, instructionY, (int)(20*scale), GREEN);
            }
            break;

        case TUT_MECH_INTRO:
            if (DrawTutWindow("VEHICLE MAINTENANCE", "Your car takes damage over time.\nIf health hits 0, you pay heavy towing fees.\n\nVisit the MECHANIC (Wrench Icon)\nto repair damage and buy performance upgrades.", true)) {
                currentState = TUT_MECH_ACTION; player->fuelConsumption = 0.02f;
            }
            break;

        case TUT_MECH_ACTION:
            // [FIX] Moved to Bottom
            DrawText("VISIT THE MECHANIC", sw/2 - 100*scale, instructionY, (int)(20*scale), BLUE);
            break;

        case TUT_DEALER_INTRO:
            if (DrawTutWindow("DREAM BIG", "That old van won't last forever.\n\nVisit the DEALERSHIP to buy new vehicles.\nSports cars go fast, Trucks haul heavy loads,\nand Luxury SUVs keep food hot longer.\n\nGo check it out.", true)) {
                currentState = TUT_DEALER_ACTION;
            }
            break;

        case TUT_DEALER_ACTION:
            // [FIX] Moved to Bottom
            if (GetDealershipState() == DEALERSHIP_ACTIVE) {
                DrawText("BROWSE CARS WITH ARROW KEYS", sw/2 - 150*scale, sh - 100*scale, (int)(20*scale), WHITE);
                DrawText("PRESS [ESC] TO EXIT DEALERSHIP", sw/2 - 160*scale, sh - 70*scale, (int)(20*scale), GOLD);
            } else {
                DrawText("GO TO DEALERSHIP", sw/2 - 100*scale, instructionY, (int)(20*scale), GOLD);
            }
            break;

        case TUT_OUTRO:
            if (DrawTutWindow("YOU'RE HIRED!", 
                "Congratulations, you've learned the basics.\n\n"
                "The city is yours. Deliver goods, earn cash,\n"
                "manage your fuel, and build your fleet.\n\n"
                "Good luck out there, Driver.", true)) {
                
                SkipTutorial(player, phone); 
                ShowPhoneNotification("TUTORIAL COMPLETED", GOLD);
            }
            break;
    }
}
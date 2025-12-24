#include "delivery_app.h"
#include "maps_app.h" 
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- MAP CONSTANTS (Must match your maps_app.c / game_map.h) ---
// If these are in game_map.h, you don't need to redefine them here.
#ifndef LOC_HOUSE
    #define LOC_FUEL        0
    #define LOC_FOOD        1
    #define LOC_CAFE        2
    #define LOC_BAR         3
    #define LOC_MARKET      4
    #define LOC_SUPERMARKET 5
    #define LOC_RESTAURANT  6
    #define LOC_HOUSE       7
#endif

// --- COLORS ---
#define COLOR_APP_BG        (Color){ 20, 24, 30, 255 }
#define COLOR_CARD_BG       (Color){ 35, 40, 50, 255 }
#define COLOR_ACCENT        (Color){ 0, 200, 83, 255 }   // Green
#define COLOR_WARN          (Color){ 255, 171, 0, 255 }  // Amber
#define COLOR_DANGER        (Color){ 213, 0, 0, 255 }    // Red
#define COLOR_TEXT_MAIN     (Color){ 240, 240, 240, 255 }
#define COLOR_TEXT_SUB      (Color){ 160, 170, 180, 255 }
#define COLOR_BUTTON        (Color){ 60, 80, 180, 255 }

typedef enum {
    SCREEN_HOME,
    SCREEN_DETAILS,
    SCREEN_PROFILE
} AppScreenState;

// --- Internal State ---
static AppScreenState currentScreen = SCREEN_HOME;
static int selectedJobIndex = -1;

// --- Helper Functions ---
extern bool GuiButton(Rectangle rect, const char *text, Color baseColor, Vector2 localMouse, bool isPressed);

// --- 1. WHITELIST LOGIC ---
// Only returns TRUE for valid pickup locations
static bool IsValidStoreType(int type) {
    switch (type) {
        case LOC_FOOD:
        case LOC_CAFE:
        case LOC_BAR:
        case LOC_MARKET:
        case LOC_SUPERMARKET:
        case LOC_RESTAURANT:
            return true;
        default:
            return false; // Blocks LOC_FUEL, LOC_HOUSE, etc.
    }
}

// Finds a random location index that matches the Whitelist
static int GetRandomStoreIndex(GameMap *map) {
    if (map->locationCount == 0) return -1;
    
    // Attempt 50 times to find a valid store (prevents infinite loops if map is empty)
    for(int i=0; i<50; i++) {
        int idx = GetRandomValue(0, map->locationCount - 1);
        if (IsValidStoreType(map->locations[idx].type)) {
            return idx;
        }
    }
    return -1;
}

static int GetRandomHouseIndex(GameMap *map) {
    if (map->locationCount == 0) return -1;
    for(int i=0; i<50; i++) {
        int idx = GetRandomValue(0, map->locationCount - 1);
        if (map->locations[idx].type == LOC_HOUSE) return idx;
    }
    return -1;
}

// --- 2. LOGIC GENERATOR BY TYPE ---
static void GenerateJobDetails(DeliveryTask *t, int locationType) {
    t->creationTime = GetTime();
    t->refreshTimer = GetRandomValue(120, 300); 
    t->fragility = 0.0f;
    t->isHeavy = false;
    t->timeLimit = 0; 
    t->jobType = locationType; // Store the actual map type

    float difficultyMult = 1.0f;

    // Logic strictly based on LOC_ constants
    switch(locationType) {
        case LOC_FOOD: // Fast Food
            snprintf(t->description, 64, "Hot Food - RUSH!");
            t->timeLimit = 180.0f; // 3 mins
            difficultyMult = 1.1f;
            break;

        case LOC_RESTAURANT: // Sit-down Restaurant
            snprintf(t->description, 64, "Fine Dining - Gentle Drive");
            t->fragility = (float)GetRandomValue(30, 60) / 100.0f; // 0.3 to 0.6
            difficultyMult = 1.3f;
            break;

        case LOC_CAFE: // Coffee
            snprintf(t->description, 64, "Hot Coffee - Fast & Stable");
            t->timeLimit = 240.0f; 
            t->fragility = 0.2f;
            difficultyMult = 1.15f;
            break;

        case LOC_BAR: // Drinks
            snprintf(t->description, 64, "Drinks - EXTREME SPILL RISK");
            t->fragility = (float)GetRandomValue(70, 95) / 100.0f; 
            difficultyMult = 2.0f; 
            break;

        case LOC_SUPERMARKET: // Supermarket
            snprintf(t->description, 64, "Groceries - Heavy Load");
            t->isHeavy = true;
            difficultyMult = 1.2f;
            break;

        case LOC_MARKET: // Local Market
        default:
            snprintf(t->description, 64, "General Goods - Standard");
            break;
    }

    // Calculate Pay
    t->pay = (12.0f + (t->distance * 0.15f)) * difficultyMult;
}

void InitDeliveryApp(PhoneState *phone, GameMap *map) {
    for(int i=0; i<5; i++) {
        phone->tasks[i].status = JOB_DELIVERED; 
        phone->tasks[i].creationTime = -9999; // Force refresh
    }
}

// --- Drawing Functions ---

static void DrawProfileScreen(Player *player, Rectangle screenRect, Vector2 mouse, bool click) {
    DrawRectangleRec(screenRect, COLOR_APP_BG);
    
    // Header
    DrawText("DRIVER STATS", screenRect.x + 20, screenRect.y + 30, 24, COLOR_ACCENT);

    // Stats Logic (Now using real player data)
    float y = screenRect.y + 80;
    
    // Helper to draw a stat row
    const char* labels[] = { "Current Balance", "Lifetime Earnings", "Total Deliveries", "Rating" };
    
    // Convert numbers to strings
    char sBal[32], sEarn[32], sDel[32];
    sprintf(sBal, "$%.2f", player->money);
    sprintf(sEarn, "$%.2f", player->totalEarnings); 
    sprintf(sDel, "%d", player->totalDeliveries);
    
    const char* values[] = { sBal, sEarn, sDel, "5.0 Stars" };

    for(int i=0; i<4; i++) {
        Rectangle row = {screenRect.x + 20, y, screenRect.width - 40, 60};
        DrawRectangleRounded(row, 0.2f, 4, COLOR_CARD_BG);
        
        DrawText(labels[i], row.x + 15, row.y + 10, 14, COLOR_TEXT_SUB);
        DrawText(values[i], row.x + 15, row.y + 30, 20, COLOR_TEXT_MAIN);
        y += 70;
    }

    // Back Button
    Rectangle btnBack = { screenRect.x + 20, screenRect.height - 70, screenRect.width - 40, 50 };
    if (GuiButton(btnBack, "BACK", COLOR_BUTTON, mouse, click)) {
        currentScreen = SCREEN_HOME;
    }
}

static void DrawJobDetails(PhoneState *phone, GameMap *map, Rectangle screenRect, Vector2 mouse, bool click) {
    DeliveryTask *t = &phone->tasks[selectedJobIndex];

    DrawRectangleRec(screenRect, COLOR_APP_BG);

    // Shop Name & Type
    DrawText(t->restaurant, screenRect.x + 20, screenRect.y + 40, 26, COLOR_TEXT_MAIN);
    DrawText(t->description, screenRect.x + 20, screenRect.y + 75, 16, COLOR_WARN);

    // Details Box
    Rectangle detailCard = { screenRect.x + 20, screenRect.y + 110, screenRect.width - 40, 200 };
    DrawRectangleRounded(detailCard, 0.1f, 6, COLOR_CARD_BG);

    float tx = detailCard.x + 15;
    float ty = detailCard.y + 15;
    
    DrawText(TextFormat("Pay: $%.2f", t->pay), tx, ty, 22, COLOR_ACCENT); ty += 35;
    DrawText(TextFormat("Dist: %.0fm", t->distance), tx, ty, 18, COLOR_TEXT_MAIN); ty += 35;
    
    DrawLine(tx, ty, detailCard.x + detailCard.width - 15, ty, GRAY); ty += 15;

    // Modifiers
    if (t->fragility > 0.0f) {
        Color riskColor = (t->fragility > 0.6f) ? COLOR_DANGER : COLOR_WARN;
        DrawText(TextFormat("Spill Risk: %d%%", (int)(t->fragility*100)), tx, ty, 18, riskColor); ty += 25;
    }
    if (t->isHeavy) {
        DrawText("HEAVY LOAD", tx, ty, 18, COLOR_WARN); ty += 25;
    }
    if (t->timeLimit > 0) {
        int min = (int)t->timeLimit / 60;
        DrawText(TextFormat("Target: %d min", min), tx, ty, 18, COLOR_ACCENT);
    }

    // Actions
    float btnY = screenRect.height - 130;
    
    if (GuiButton((Rectangle){screenRect.x + 20, btnY, screenRect.width - 40, 45}, "Show on Map", COLOR_BUTTON, mouse, click)) {
         phone->currentApp = APP_MAP;
         Vector2 target = (t->status == JOB_PICKED_UP) ? t->customerPos : t->restaurantPos;
         SetMapDestination(map, target);
    }
    
    btnY += 55;

    bool isActive = (t->status != JOB_AVAILABLE);
    const char* txt = isActive ? "ABANDON JOB" : "ACCEPT JOB";
    Color col = isActive ? COLOR_DANGER : COLOR_ACCENT;

    if (GuiButton((Rectangle){screenRect.x + 20, btnY, screenRect.width - 40, 45}, txt, col, mouse, click)) {
        if (!isActive) {
             // Accept Logic: Clear others
             for(int j=0; j<5; j++) {
                if (phone->tasks[j].status == JOB_AVAILABLE) phone->tasks[j].status = JOB_AVAILABLE; 
             }
            t->status = JOB_ACCEPTED;
            currentScreen = SCREEN_HOME; 
        } else {
            t->status = JOB_AVAILABLE; 
        }
    }

    // Close
    if (GuiButton((Rectangle){screenRect.width - 45, 20, 30, 30}, "X", COLOR_CARD_BG, mouse, click)) {
        currentScreen = SCREEN_HOME;
    }
}

static void DrawHomeScreen(PhoneState *phone, Player *player, Rectangle screenRect, Vector2 mouse, bool click) {
    // 1. Profile Header
    Rectangle headerRect = { 0, 0, SCREEN_WIDTH, 85 };
    DrawRectangleRec(headerRect, COLOR_CARD_BG);
    
    // Avatar
    DrawCircle(45, 42, 28, LIGHTGRAY);
    
    // Stats (Live from player struct now)
    DrawText("Driver", 85, 25, 18, COLOR_TEXT_MAIN);
    DrawText(TextFormat("Wallet: $%.2f", player->money), 85, 48, 16, COLOR_ACCENT);

    // Profile Click
    if (CheckCollisionPointRec(mouse, headerRect) && click) {
        currentScreen = SCREEN_PROFILE;
    }

    // 2. Job Feed
    float y = 100;
    DrawText("Available Deliveries", 20, y, 16, COLOR_TEXT_SUB);
    y += 25;

    for (int i = 0; i < 5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        
        // Hide if just finished
        if (t->status == JOB_DELIVERED && GetTime() - t->creationTime < 3.0) continue; 
        
        Rectangle cardRect = { 10, y, SCREEN_WIDTH - 20, 90 };
        Color cardColor = (t->status == JOB_ACCEPTED || t->status == JOB_PICKED_UP) ? (Color){30, 50, 30, 255} : COLOR_CARD_BG;

        DrawRectangleRounded(cardRect, 0.2f, 4, cardColor);
        
        // Click to Details
        if (GuiButton(cardRect, "", BLANK, mouse, click)) { 
            selectedJobIndex = i;
            currentScreen = SCREEN_DETAILS;
        }

        // Text Info
        DrawText(t->restaurant, cardRect.x + 15, cardRect.y + 12, 20, COLOR_TEXT_MAIN);
        DrawText(t->description, cardRect.x + 15, cardRect.y + 38, 14, COLOR_TEXT_SUB);

        // Price Tag
        const char* price = TextFormat("$%.0f", t->pay);
        int pw = MeasureText(price, 20);
        DrawRectangleRounded((Rectangle){cardRect.x + cardRect.width - pw - 20, cardRect.y + 10, pw + 10, 26}, 0.5f, 4, COLOR_ACCENT);
        DrawText(price, cardRect.x + cardRect.width - pw - 15, cardRect.y + 13, 20, WHITE);

        // Status
        const char* dist = TextFormat("%.1fkm", t->distance/1000.0f);
        if(t->status == JOB_ACCEPTED) dist = "PICK UP";
        if(t->status == JOB_PICKED_UP) dist = "DELIVERING";
        
        DrawText(dist, cardRect.x + cardRect.width - MeasureText(dist, 14) - 15, cardRect.y + 65, 14, (t->status != JOB_AVAILABLE)?COLOR_ACCENT:GRAY);

        y += 100;
    }
}

// --- MAIN FUNCTIONS ---

// NOTE: We pass 'Player *player' here. 
// Previously we were creating a fake player inside.
void DrawDeliveryApp(PhoneState *phone, Player *player, GameMap *map, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_APP_BG);
    
    Rectangle screenRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

    

    switch(currentScreen) {
        case SCREEN_HOME:
            DrawHomeScreen(phone, player, screenRect, mouse, click);
            break;
        case SCREEN_DETAILS:
            DrawJobDetails(phone, map, screenRect, mouse, click);
            break;
        case SCREEN_PROFILE:
            DrawProfileScreen(player, screenRect, mouse, click);
            break;
    }
}

void UpdateDeliveryApp(PhoneState *phone, Player *player, GameMap *map) {
    double currentTime = GetTime();

    for(int i=0; i<5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        
        // --- 1. REFRESH LOGIC ---
        if (t->status == JOB_DELIVERED) {
             // Try to find valid locations
             int storeIdx = GetRandomStoreIndex(map);
             int houseIdx = GetRandomHouseIndex(map);
             
             // Only generate if we found valid whitelisted locations
             if (storeIdx != -1 && houseIdx != -1) {
                snprintf(t->restaurant, 32, "%s", map->locations[storeIdx].name);
                t->restaurantPos = map->locations[storeIdx].position;
                
                snprintf(t->customer, 32, "House #%d", houseIdx);
                t->customerPos = map->locations[houseIdx].position;
                
                t->distance = Vector2Distance(t->restaurantPos, t->customerPos);
                t->status = JOB_AVAILABLE;
                
                // Pass the VALIDATED type (from map) to the generator
                GenerateJobDetails(t, map->locations[storeIdx].type);
             }
        }
        
        // Expiration
        if (t->status == JOB_AVAILABLE && (currentTime - t->creationTime > t->refreshTimer)) {
            if (GetRandomValue(0, 100) < 2) t->status = JOB_DELIVERED; 
        }

        // --- 2. PHYSICS & COMPLETION LOGIC ---
        if (t->status == JOB_AVAILABLE || t->status == JOB_DELIVERED) continue;

        Vector2 playerPos2D = { player->position.x, player->position.z };

        // Pickup
        if (t->status == JOB_ACCEPTED) {
            if (Vector2Distance(playerPos2D, t->restaurantPos) < 10.0f) {
                t->status = JOB_PICKED_UP;
                SetMapDestination(map, t->customerPos);
            }
        }
        // Delivery
        else if (t->status == JOB_PICKED_UP) {
            if (Vector2Distance(playerPos2D, t->customerPos) < 10.0f) {
                t->status = JOB_DELIVERED;
                
                // --- PAYOUT & STATS UPDATE ---
                // 1. Use your helper to update money AND bank history
                AddMoney(player, t->restaurant, t->pay);
        
                // 2. Update stats manually (since AddMoney doesn't know about stats)
                player->totalEarnings += t->pay;
                player->totalDeliveries++;
                
                // Tip Logic
                if (t->timeLimit > 0 && (currentTime - t->creationTime) < t->timeLimit) {
                    float tip = t->pay * 0.2f;
                    player->money += tip;
                    player->totalEarnings += tip;
                }
            }
        }
    }
}
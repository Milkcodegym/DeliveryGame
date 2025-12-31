#include "delivery_app.h"
#include "maps_app.h" 
#include "map.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- EXTERNAL ---
extern void ShowPhoneNotification(const char *text, Color color);

// --- MAP CONSTANTS ---
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

#define COLOR_APP_BG        (Color){ 20, 24, 30, 255 }
#define COLOR_CARD_BG       (Color){ 35, 40, 50, 255 }
#define COLOR_ACCENT        (Color){ 0, 200, 83, 255 } 
#define COLOR_WARN          (Color){ 255, 171, 0, 255 }
#define COLOR_DANGER        (Color){ 213, 0, 0, 255 } 
#define COLOR_TEXT_MAIN     (Color){ 240, 240, 240, 255 }
#define COLOR_TEXT_SUB      (Color){ 160, 170, 180, 255 }
#define COLOR_BUTTON        (Color){ 60, 80, 180, 255 }

typedef enum {
    SCREEN_HOME,
    SCREEN_DETAILS,
    SCREEN_PROFILE
} AppScreenState;

static AppScreenState currentScreen = SCREEN_HOME;
static int selectedJobIndex = -1;
static float eventFallbackTimer = 120.0f; // 2 min fallback

extern bool GuiButton(Rectangle rect, const char *text, Color baseColor, Vector2 localMouse, bool isPressed);

static bool IsValidStoreType(int type) {
    switch (type) {
        case LOC_FOOD: case LOC_CAFE: case LOC_BAR:
        case LOC_MARKET: case LOC_SUPERMARKET: case LOC_RESTAURANT:
            return true;
        default: return false; 
    }
}

static int GetRandomStoreIndex(GameMap *map) {
    if (map->locationCount == 0) return -1;
    for(int i=0; i<50; i++) {
        int idx = GetRandomValue(0, map->locationCount - 1);
        if (IsValidStoreType(map->locations[idx].type)) return idx;
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

static void GenerateJobDetails(DeliveryTask *t, int locationType) {
    t->creationTime = GetTime();
    t->refreshTimer = GetRandomValue(120, 300); 
    t->fragility = 0.0f;
    t->isHeavy = false;
    t->timeLimit = 0; 
    t->jobType = locationType; 
    float difficultyMult = 1.0f;

    switch(locationType) {
        case LOC_FOOD:
            snprintf(t->description, 64, "Hot Food - RUSH!");
            t->timeLimit = 180.0f; difficultyMult = 1.1f; break;
        case LOC_RESTAURANT:
            snprintf(t->description, 64, "Fine Dining - Gentle Drive");
            t->fragility = (float)GetRandomValue(30, 60) / 100.0f; difficultyMult = 1.3f; break;
        case LOC_CAFE:
            snprintf(t->description, 64, "Hot Coffee - Fast & Stable");
            t->timeLimit = 240.0f; t->fragility = 0.2f; difficultyMult = 1.15f; break;
        case LOC_BAR:
            snprintf(t->description, 64, "Drinks - EXTREME SPILL RISK");
            t->fragility = (float)GetRandomValue(70, 95) / 100.0f; difficultyMult = 2.0f; break;
        case LOC_SUPERMARKET:
            snprintf(t->description, 64, "Groceries - Heavy Load");
            t->isHeavy = true; difficultyMult = 1.2f; break;
        case LOC_MARKET: default:
            snprintf(t->description, 64, "General Goods - Standard"); break;
    }
    t->pay = (12.0f + (t->distance * 0.15f)) * difficultyMult;
}

void InitDeliveryApp(PhoneState *phone, GameMap *map) {
    for(int i=0; i<5; i++) {
        phone->tasks[i].status = JOB_DELIVERED; 
        phone->tasks[i].creationTime = -9999;
    }
}

// ... DrawProfileScreen, DrawJobDetails, DrawHomeScreen (Keeping short for context, but same logic) ...
// (I will assume these exist unchanged except for trigger calls below)

static void DrawProfileScreen(Player *player, Rectangle screenRect, Vector2 mouse, bool click) {
    DrawRectangleRec(screenRect, COLOR_APP_BG);
    DrawText("DRIVER STATS", screenRect.x + 20, screenRect.y + 30, 24, COLOR_ACCENT);
    float y = screenRect.y + 80;
    const char* labels[] = { "Current Balance", "Lifetime Earnings", "Total Deliveries", "Rating" };
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
    Rectangle btnBack = { screenRect.x + 20, screenRect.height - 70, screenRect.width - 40, 50 };
    if (GuiButton(btnBack, "BACK", COLOR_BUTTON, mouse, click)) currentScreen = SCREEN_HOME;
}

static void DrawJobDetails(PhoneState *phone, GameMap *map, Rectangle screenRect, Vector2 mouse, bool click) {
    DeliveryTask *t = &phone->tasks[selectedJobIndex];
    DrawRectangleRec(screenRect, COLOR_APP_BG);
    DrawText(t->restaurant, screenRect.x + 20, screenRect.y + 40, 26, COLOR_TEXT_MAIN);
    DrawText(t->description, screenRect.x + 20, screenRect.y + 75, 16, COLOR_WARN);
    
    Rectangle detailCard = { screenRect.x + 20, screenRect.y + 110, screenRect.width - 40, 200 };
    DrawRectangleRounded(detailCard, 0.1f, 6, COLOR_CARD_BG);
    float tx = detailCard.x + 15; float ty = detailCard.y + 15;
    DrawText(TextFormat("Pay: $%.2f", t->pay), tx, ty, 22, COLOR_ACCENT); ty += 35;
    DrawText(TextFormat("Dist: %.0fm", t->distance), tx, ty, 18, COLOR_TEXT_MAIN); ty += 35;
    DrawLine(tx, ty, detailCard.x + detailCard.width - 15, ty, GRAY); ty += 15;
    if (t->fragility > 0.0f) {
        Color riskColor = (t->fragility > 0.6f) ? COLOR_DANGER : COLOR_WARN;
        DrawText(TextFormat("Spill Risk: %d%%", (int)(t->fragility*100)), tx, ty, 18, riskColor); ty += 25;
    }
    if (t->isHeavy) { DrawText("HEAVY LOAD", tx, ty, 18, COLOR_WARN); ty += 25; }
    if (t->timeLimit > 0) {
        int min = (int)t->timeLimit / 60;
        DrawText(TextFormat("Target: %d min", min), tx, ty, 18, COLOR_ACCENT);
    }

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
             for(int j=0; j<5; j++) {
                if (phone->tasks[j].status == JOB_AVAILABLE) phone->tasks[j].status = JOB_AVAILABLE; 
             }
            t->status = JOB_ACCEPTED;
            SetMapDestination(map, t->restaurantPos);
            ShowPhoneNotification("New Job Accepted", COLOR_ACCENT);
            
            // TRIGGER EVENT ON START
            // Placeholder: Need player info to calc fwd, passing 0,0,0 causes basic check
            // Ideally UpdateDeliveryApp handles this via player struct
            
            currentScreen = SCREEN_HOME; 
        } else {
            t->status = JOB_AVAILABLE; 
            ShowPhoneNotification("Job Cancelled", COLOR_DANGER);
        }
    }
    if (GuiButton((Rectangle){screenRect.width - 45, 20, 30, 30}, "X", COLOR_CARD_BG, mouse, click)) currentScreen = SCREEN_HOME;
}

static void DrawHomeScreen(PhoneState *phone, Player *player, Rectangle screenRect, Vector2 mouse, bool click) {
    Rectangle headerRect = { 0, 0, SCREEN_WIDTH, 85 };
    DrawRectangleRec(headerRect, COLOR_CARD_BG);
    DrawCircle(45, 42, 28, LIGHTGRAY);
    DrawText("Driver", 85, 25, 18, COLOR_TEXT_MAIN);
    DrawText(TextFormat("Wallet: $%.2f", player->money), 85, 48, 16, COLOR_ACCENT);
    if (CheckCollisionPointRec(mouse, headerRect) && click) currentScreen = SCREEN_PROFILE;

    float y = 100;
    DrawText("Available Deliveries", 20, y, 16, COLOR_TEXT_SUB);
    y += 25;
    for (int i = 0; i < 5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        if (t->status == JOB_DELIVERED && GetTime() - t->creationTime < 3.0) continue; 
        Rectangle cardRect = { 10, y, SCREEN_WIDTH - 20, 90 };
        Color cardColor = (t->status == JOB_ACCEPTED || t->status == JOB_PICKED_UP) ? (Color){30, 50, 30, 255} : COLOR_CARD_BG;
        DrawRectangleRounded(cardRect, 0.2f, 4, cardColor);
        if (GuiButton(cardRect, "", BLANK, mouse, click)) { 
            selectedJobIndex = i;
            currentScreen = SCREEN_DETAILS;
        }
        DrawText(t->restaurant, cardRect.x + 15, cardRect.y + 12, 20, COLOR_TEXT_MAIN);
        DrawText(t->description, cardRect.x + 15, cardRect.y + 38, 14, COLOR_TEXT_SUB);
        const char* price = TextFormat("$%.0f", t->pay);
        int pw = MeasureText(price, 20);
        DrawRectangleRounded((Rectangle){cardRect.x + cardRect.width - pw - 20, cardRect.y + 10, pw + 10, 26}, 0.5f, 4, COLOR_ACCENT);
        DrawText(price, cardRect.x + cardRect.width - pw - 15, cardRect.y + 13, 20, WHITE);
        const char* dist = TextFormat("%.1fkm", t->distance/1000.0f);
        if(t->status == JOB_ACCEPTED) dist = "PICK UP";
        if(t->status == JOB_PICKED_UP) dist = "DELIVERING";
        DrawText(dist, cardRect.x + cardRect.width - MeasureText(dist, 14) - 15, cardRect.y + 65, 14, (t->status != JOB_AVAILABLE)?COLOR_ACCENT:GRAY);
        y += 100;
    }
}

void DrawDeliveryApp(PhoneState *phone, Player *player, GameMap *map, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_APP_BG);
    Rectangle screenRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    switch(currentScreen) {
        case SCREEN_HOME: DrawHomeScreen(phone, player, screenRect, mouse, click); break;
        case SCREEN_DETAILS: DrawJobDetails(phone, map, screenRect, mouse, click); break;
        case SCREEN_PROFILE: DrawProfileScreen(player, screenRect, mouse, click); break;
    }
}

void UpdateDeliveryApp(PhoneState *phone, Player *player, GameMap *map) {
    double currentTime = GetTime();
    float dt = GetFrameTime();

    // Fallback Event Trigger (2 mins)
    eventFallbackTimer -= dt;
    if (eventFallbackTimer <= 0) {
        Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
        TriggerRandomEvent(map, player->position, fwd);
        eventFallbackTimer = 120.0f;
    }

    for(int i=0; i<5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        if (t->status == JOB_DELIVERED) {
             int storeIdx = GetRandomStoreIndex(map);
             int houseIdx = GetRandomHouseIndex(map);
             if (storeIdx != -1 && houseIdx != -1) {
                snprintf(t->restaurant, 32, "%s", map->locations[storeIdx].name);
                t->restaurantPos = map->locations[storeIdx].position;
                snprintf(t->customer, 32, "House #%d", houseIdx);
                t->customerPos = map->locations[houseIdx].position;
                t->distance = Vector2Distance(t->restaurantPos, t->customerPos);
                t->status = JOB_AVAILABLE;
                GenerateJobDetails(t, map->locations[storeIdx].type);
             }
        }
        if (t->status == JOB_AVAILABLE && (currentTime - t->creationTime > t->refreshTimer)) {
            if (GetRandomValue(0, 100) < 2) t->status = JOB_DELIVERED; 
        }
        if (t->status == JOB_AVAILABLE || t->status == JOB_DELIVERED) continue;

        Vector2 playerPos2D = { player->position.x, player->position.z };

        if (t->status == JOB_ACCEPTED) {
            if (Vector2Distance(playerPos2D, t->restaurantPos) < 5.0f) {
                t->status = JOB_PICKED_UP;
                SetMapDestination(map, t->customerPos);
                ShowPhoneNotification("Order Picked Up!", COLOR_ACCENT);
                
                // EVENT TRIGGER ON PICKUP
                Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
                TriggerRandomEvent(map, player->position, fwd);
                eventFallbackTimer = 120.0f; // Reset fallback
            }
        }
        else if (t->status == JOB_PICKED_UP) {
            if (Vector2Distance(playerPos2D, t->customerPos) < 5.0f) {
                t->status = JOB_DELIVERED;
                AddMoney(player, t->restaurant, t->pay);
                player->totalEarnings += t->pay;
                player->totalDeliveries++;
                if (t->timeLimit > 0 && (currentTime - t->creationTime) < t->timeLimit) {
                    float tip = t->pay * 0.2f;
                    player->money += tip;
                    player->totalEarnings += tip;
                    ShowPhoneNotification(TextFormat("Paid $%.2f + Tip!", t->pay + tip), GREEN);
                } else {
                    ShowPhoneNotification(TextFormat("Paid $%.2f", t->pay), GREEN);
                }
                
                // EVENT TRIGGER ON DELIVERY
                Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
                TriggerRandomEvent(map, player->position, fwd);
                eventFallbackTimer = 120.0f; // Reset fallback
            }
        }
    }
}
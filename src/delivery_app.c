#include "delivery_app.h"
#include "save.h"
#include "maps_app.h" 
#include "map.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // Added for fmax/fmin if needed

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
static float eventFallbackTimer = 120.0f; 

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

static void DrawJobDetails(PhoneState *phone, Player *player, GameMap *map, Rectangle screenRect, Vector2 mouse, bool click) {
    DeliveryTask *t = &phone->tasks[selectedJobIndex];
    DrawRectangleRec(screenRect, COLOR_APP_BG);
    DrawText(t->restaurant, screenRect.x + 20, screenRect.y + 40, 26, COLOR_TEXT_MAIN);
    DrawText(t->description, screenRect.x + 20, screenRect.y + 75, 16, COLOR_WARN);
    
    Rectangle detailCard = { screenRect.x + 20, screenRect.y + 110, screenRect.width - 40, 220 }; // Slightly taller
    DrawRectangleRounded(detailCard, 0.1f, 6, COLOR_CARD_BG);
    float tx = detailCard.x + 15; float ty = detailCard.y + 15;
    DrawText(TextFormat("Pay: $%.2f", t->pay), tx, ty, 22, COLOR_ACCENT); ty += 35;
    DrawText(TextFormat("Dist: %.0fm", t->distance), tx, ty, 18, COLOR_TEXT_MAIN); ty += 35;
    DrawLine(tx, ty, detailCard.x + detailCard.width - 15, ty, GRAY); ty += 15;
    
    // --- DETAILS ---
    if (t->fragility > 0.0f) {
        Color riskColor = (t->fragility > 0.6f) ? COLOR_DANGER : COLOR_WARN;
        DrawText(TextFormat("Spill Risk: %d%%", (int)(t->fragility*100)), tx, ty, 18, riskColor); ty += 25;
    }
    
    if (t->isHeavy) { 
        DrawText("HEAVY LOAD", tx, ty, 18, COLOR_WARN); 
        // Warning if player car is weak
        if (player->loadResistance > 0.6f) {
            DrawText("(Vehicle not suited!)", tx + 120, ty, 16, COLOR_DANGER);
        }
        ty += 25; 
    }
    
    if (t->timeLimit > 0) {
        int min = (int)t->timeLimit / 60;
        DrawText(TextFormat("Target: %d min", min), tx, ty, 18, COLOR_ACCENT);
        // Insulation bonus info
        if (player->insulationFactor < 0.9f) {
             DrawText("(Insulated)", tx + 120, ty, 16, SKYBLUE);
        }
    }

    float btnY = screenRect.height - 130;
    
    if (GuiButton((Rectangle){screenRect.x + 20, btnY, screenRect.width - 40, 45}, "Show on Map", COLOR_BUTTON, mouse, click)) {
         phone->currentApp = APP_MAP;
         Vector2 target = (t->status == JOB_PICKED_UP) ? t->customerPos : t->restaurantPos;
         
         if (t->status == JOB_AVAILABLE) {
             PreviewMapLocation(map, target);
         } else {
             SetMapDestination(map, target);
         }
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
        case SCREEN_DETAILS: DrawJobDetails(phone, player, map, screenRect, mouse, click); break; // Updated to pass Player
        case SCREEN_PROFILE: DrawProfileScreen(player, screenRect, mouse, click); break;
    }
}

void UpdateDeliveryApp(PhoneState *phone, Player *player, GameMap *map) {
    double currentTime = GetTime();
    float dt = GetFrameTime();

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

        if (t->status == JOB_PICKED_UP) {
            Vector2 playerPos2D = { player->position.x, player->position.z };

            // --- G-FORCE & FRAGILITY LOGIC ---
            if (t->fragility > 0.0f) {
                float turnG = (fabs(player->current_speed) * 0.02f) * (IsKeyDown(KEY_A) || IsKeyDown(KEY_D) ? 1.0f : 0.0f);
                float brakeG = (player->brake_power > 0) ? 0.8f : 0.0f;
                float currentG = turnG + brakeG;

                float gLimit = 1.5f * (1.0f - t->fragility);
                if (gLimit < 0.3f) gLimit = 0.3f;

                if (currentG > gLimit) {
                    float penalty = 5.0f * dt; 
                    t->pay -= penalty;
                    if (GetRandomValue(0, 30) == 0) { 
                        ShowPhoneNotification("CARGO DAMAGED!", COLOR_DANGER);
                    }
                }
            }
            
            // --- HEAVY LOAD LOGIC [NEW] ---
            if (t->isHeavy) {
                // If the player's car has high Load Sensitivity (e.g. 1.0+), they take damage/penalty for driving recklessly
                // Trucks have 0.1 sensitivity, so this block rarely triggers for them.
                if (player->loadResistance > 0.5f) {
                    // Penalty scales with speed for unsuited vehicles
                    float heavyPenalty = (fabs(player->current_speed) * 0.05f) * player->loadResistance * dt;
                    // Deduct from pay (cargo damage) 
                    t->pay -= heavyPenalty * 0.1f; 
                }
            }

            // --- TEMPERATURE & INSULATION LOGIC [NEW] ---
            if (t->timeLimit > 0) {
                // Determine how much "Food Time" has passed relative to "Real Time"
                // InsulationFactor: 1.0 = Normal, 0.5 = Keeps hot twice as long
                double realElapsed = currentTime - t->creationTime; 
                double foodElapsed = realElapsed * player->insulationFactor;

                if (foodElapsed > t->timeLimit) {
                    float penalty = 2.0f * dt;
                    t->pay -= penalty;
                }
            }
            
            if (t->pay < 5.0f) t->pay = 5.0f;

            // --- DELIVERY COMPLETION ---
            if (Vector2Distance(playerPos2D, t->customerPos) < 5.0f) {
                t->status = JOB_DELIVERED;
                ShowPhoneNotification("Delivered!", GREEN);
                AddMoney(player, t->restaurant, t->pay);
                player->totalEarnings += t->pay;
                player->totalDeliveries++;
                
                // Calculate Tip based on Insulation success
                double realElapsed = currentTime - t->creationTime;
                double effectiveElapsed = realElapsed * player->insulationFactor;

                if (t->timeLimit > 0 && effectiveElapsed < t->timeLimit) {
                    float tip = t->pay * 0.2f;
                    
                    // Bonus tip for Fragile goods handled well
                    if (t->fragility > 0.5f) tip += 15.0f;
                    
                    // Bonus tip if delivered VERY fast (or with great insulation)
                    if (effectiveElapsed < t->timeLimit * 0.5f) tip += 10.0f;

                    player->money += tip;
                    player->totalEarnings += tip;
                    ShowPhoneNotification(TextFormat("Paid $%.2f + Tip!", t->pay + tip), GREEN);
                } else {
                    ShowPhoneNotification(TextFormat("Paid $%.2f", t->pay), GREEN);
                }
                
                Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
                TriggerRandomEvent(map, player->position, fwd);
                eventFallbackTimer = 120.0f; 
                SaveGame(player, phone);
                ShowPhoneNotification("Auto-Saved", LIME);
            }
        }
        else if (t->status == JOB_ACCEPTED) {
            Vector2 playerPos2D = { player->position.x, player->position.z };
            if (Vector2Distance(playerPos2D, t->restaurantPos) < 5.0f) {
                t->status = JOB_PICKED_UP;
                t->creationTime = currentTime; // Timer starts now
                SetMapDestination(map, t->customerPos);
                ShowPhoneNotification("Order Picked Up!", COLOR_ACCENT);
                
                Vector3 fwd = { sinf(player->angle*DEG2RAD), 0, cosf(player->angle*DEG2RAD) };
                TriggerRandomEvent(map, player->position, fwd);
                eventFallbackTimer = 120.0f;
            }
        }
    }
}
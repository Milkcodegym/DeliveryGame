#include "phone.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h> 

#include "player.h" 
#include "map.h"
#include "maps_app.h" 

// --- Helper Forward Declaration ---
bool GuiButton(Rectangle rect, const char *text, Color baseColor, Vector2 localMouse, bool isPressed);

// Helper to find a random location of a specific type
int GetRandomLocationIndex(GameMap *map, bool isStore) {
    if (map->locationCount == 0) return -1;
    
    for(int i=0; i<100; i++) {
        int idx = GetRandomValue(0, map->locationCount - 1);
        bool isHouse = (map->locations[idx].type == LOC_HOUSE);
        
        if (isStore && !isHouse) return idx; 
        if (!isStore && isHouse) return idx; 
    }
    return -1; 
}

void InitPhone(PhoneState *phone, GameMap *map) {
    phone->screenTexture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    phone->currentApp = APP_HOME;
    phone->isOpen = true;
    phone->slideAnim = 1.0f;
    
    InitMapsApp(); 

    // --- Generate Jobs ---
    for(int i=0; i<5; i++) {
        int storeIdx = GetRandomLocationIndex(map, true); 
        int houseIdx = GetRandomLocationIndex(map, false); 
        
        if (storeIdx != -1 && houseIdx != -1) {
            DeliveryTask *t = &phone->tasks[i];
            
            snprintf(t->restaurant, 32, "%s", map->locations[storeIdx].name);
            t->restaurantPos = map->locations[storeIdx].position;
            
            snprintf(t->customer, 32, "House #%d", houseIdx); 
            t->customerPos = map->locations[houseIdx].position;
            
            float dist = Vector2Distance(t->restaurantPos, t->customerPos);
            t->distance = dist; 
            t->pay = 15.0f + (dist * 0.2f); // Better pay formula
            t->status = JOB_AVAILABLE;
        } else {
            snprintf(phone->tasks[i].restaurant, 32, "No Job");
            phone->tasks[i].status = JOB_DELIVERED; 
        }
    }

    // Init Music
    phone->music.library[0] = (Song){"Banger beat", "Milk", "resources/music/song1.ogg", {0}, 0};
    phone->music.library[1] = (Song){"Fire track", "Coolartist", "resources/music/song2.ogg", {0}, 0};
    phone->music.library[2] = (Song){"Melodic tune", "Litsolou19", "resources/music/song3.ogg", {0}, 0};

    for(int i=0; i<3; i++) {
        phone->music.library[i].stream = LoadMusicStream(phone->music.library[i].filePath);
        phone->music.library[i].duration = GetMusicTimeLength(phone->music.library[i].stream);
        phone->music.library[i].stream.looping = true; 
    }
    phone->music.currentSongIdx = 0;
    phone->music.isPlaying = false;

    // Init Settings
    phone->settings.masterVolume = 0.8f;
    phone->settings.sfxVolume = 1.0f;
    phone->settings.mute = false;
}

bool GuiButton(Rectangle rect, const char *text, Color baseColor, Vector2 localMouse, bool isPressed) {
    bool hovered = CheckCollisionPointRec(localMouse, rect);
    Color color = hovered ? Fade(baseColor, 0.8f) : baseColor;
    if (hovered && isPressed) color = DARKGRAY;

    DrawRectangleRec(rect, color);
    DrawRectangleLinesEx(rect, 2, DARKGRAY);
    
    int textW = MeasureText(text, 20);
    DrawText(text, rect.x + (rect.width - textW)/2, rect.y + (rect.height - 20)/2, 20, WHITE);

    return (hovered && isPressed);
}

void DrawAppHome(PhoneState *phone, Vector2 mouse, bool click) {
    const char* icons[] = { "Jobs", "Maps", "Bank", "Music", "Settings", "Web" };
    Color colors[] = { ORANGE, BLUE, GREEN, PURPLE, GRAY, SKYBLUE };
    
    int cols = 2;
    float iconSize = 90;
    float gap = 20;
    float startX = (SCREEN_WIDTH - (cols*iconSize + (cols-1)*gap)) / 2;
    float startY = 80;

    for (int i = 0; i < 6; i++) {
        int col = i % cols;
        int row = i / cols;
        Rectangle btn = { startX + col*(iconSize+gap), startY + row*(iconSize+gap), iconSize, iconSize };
        
        if (GuiButton(btn, icons[i], colors[i], mouse, click)) {
            if (i == 0) phone->currentApp = APP_DELIVERY;
            if (i == 1) phone->currentApp = APP_MAP; 
            if (i == 2) phone->currentApp = APP_BANK;
            if (i == 3) phone->currentApp = APP_MUSIC;
            if (i == 4) phone->currentApp = APP_SETTINGS;
            if (i == 5) phone->currentApp = APP_BROWSER;
        }
    }
}

void DrawAppDelivery(PhoneState *phone, GameMap *map, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 60, ORANGE);
    DrawText("DELIVERY JOBS", 20, 20, 20, WHITE);
    
    float y = 70;
    for (int i = 0; i < 5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        if (t->status == JOB_DELIVERED) continue; 

        Rectangle rowRect = { 10, y, SCREEN_WIDTH - 20, 100 }; 
        Color bg = (t->status == JOB_ACCEPTED || t->status == JOB_PICKED_UP) ? Fade(GREEN, 0.2f) : LIGHTGRAY;
        DrawRectangleRec(rowRect, bg);
        DrawRectangleLinesEx(rowRect, 1, DARKGRAY);
        
        // Dynamic Status Text
        const char* statusTxt = "AVAILABLE";
        if(t->status == JOB_ACCEPTED) statusTxt = "PICK UP";
        if(t->status == JOB_PICKED_UP) statusTxt = "DELIVERING";

        DrawText(t->restaurant, rowRect.x + 10, rowRect.y + 10, 20, BLACK);
        DrawText(TextFormat("$%.2f", t->pay), rowRect.x + 180, rowRect.y + 10, 20, DARKGREEN);
        DrawText(TextFormat("%s (%.0fm)", statusTxt, t->distance), rowRect.x + 10, rowRect.y + 35, 10, DARKGRAY);
        
        // BUTTONS
        Rectangle btnMap = { rowRect.x + 10, rowRect.y + 60, 110, 30 };
        if (GuiButton(btnMap, "Open Map", BLUE, mouse, click)) {
            phone->currentApp = APP_MAP;
            Vector2 target = (t->status == JOB_PICKED_UP) ? t->customerPos : t->restaurantPos;
            SetMapDestination(map, target);
        }

        Rectangle btnAccept = { rowRect.x + 130, rowRect.y + 60, 110, 30 };
        const char* btnText = (t->status == JOB_AVAILABLE) ? "ACCEPT" : "ACTIVE";
        Color btnColor = (t->status == JOB_AVAILABLE) ? GREEN : GRAY;
        
        if (GuiButton(btnAccept, btnText, btnColor, mouse, click)) {
            if (t->status == JOB_AVAILABLE) {
                // Reset others so only 1 is active
                for(int j=0; j<5; j++) {
                    if (phone->tasks[j].status != JOB_DELIVERED) phone->tasks[j].status = JOB_AVAILABLE;
                }
                t->status = JOB_ACCEPTED;
            }
        }
        y += 110;
    }
}

void DrawAppBank(PhoneState *phone, Player *player) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 140, DARKGREEN);
    DrawText("CURRENT BALANCE", 20, 30, 10, Fade(WHITE, 0.7f));
    DrawText(TextFormat("$%.2f", player->money), 20, 50, 40, WHITE);
    
    DrawRectangle(0, 140, SCREEN_WIDTH, 30, LIGHTGRAY);
    DrawText("RECENT ACTIVITY", 10, 148, 10, DARKGRAY);

    float y = 180;
    for (int i = 0; i < player->transactionCount; i++) {
        Transaction t = player->history[i];
        DrawText(t.description, 20, (int)y, 20, BLACK);
        Color amtColor = (t.amount >= 0) ? GREEN : RED;
        const char* sign = (t.amount >= 0) ? "+" : "";
        const char* amtText = TextFormat("%s$%.2f", sign, t.amount);
        int textW = MeasureText(amtText, 20);
        DrawText(amtText, SCREEN_WIDTH - textW - 20, (int)y, 20, amtColor);
        DrawLine(20, (int)y + 25, SCREEN_WIDTH - 20, (int)y + 25, Fade(LIGHTGRAY, 0.5f));
        y += 35;
    }
}

void DrawAppMusic(PhoneState *phone, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){20, 20, 30, 255}); 
    Song *s = &phone->music.library[phone->music.currentSongIdx];

    DrawRectangle(40, 60, 200, 200, DARKPURPLE);
    DrawText("ALBUM", 100, 150, 20, WHITE);

    DrawText(s->title, 20, 300, 24, WHITE);
    DrawText(s->artist, 20, 330, 18, GRAY);

    float timePlayed = GetMusicTimePlayed(s->stream);
    float progress = timePlayed / s->duration;
    
    float barWidth = SCREEN_WIDTH - 40;
    DrawRectangle(20, 380, barWidth, 4, GRAY);
    DrawRectangle(20, 380, barWidth * progress, 4, GREEN);
    
    DrawText(TextFormat("%02d:%02d", (int)timePlayed/60, (int)timePlayed%60), 20, 390, 10, LIGHTGRAY);

    Rectangle btnPrev = { 30, 420, 60, 60 };
    Rectangle btnPlay = { 110, 420, 60, 60 };
    Rectangle btnNext = { 190, 420, 60, 60 };

    if (GuiButton(btnPrev, "|<", DARKBLUE, mouse, click)) {
        StopMusicStream(s->stream); 
        phone->music.isPlaying = true; 
        phone->music.currentSongIdx--;
        if(phone->music.currentSongIdx < 0) phone->music.currentSongIdx = 2; 
        PlayMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
    }
    
    const char* playIcon = phone->music.isPlaying ? "||" : ">";
    Color playColor = phone->music.isPlaying ? GREEN : ORANGE;
    
    if (GuiButton(btnPlay, playIcon, playColor, mouse, click)) {
        phone->music.isPlaying = !phone->music.isPlaying;
        if (phone->music.isPlaying) ResumeMusicStream(s->stream);
        else PauseMusicStream(s->stream);
    }

    if (GuiButton(btnNext, ">|", DARKBLUE, mouse, click)) {
        StopMusicStream(s->stream); 
        phone->music.isPlaying = true; 
        phone->music.currentSongIdx++;
        if(phone->music.currentSongIdx > 2) phone->music.currentSongIdx = 0; 
        PlayMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
    }
}

void DrawAppSettings(PhoneState *phone, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 60, GRAY);
    DrawText("SETTINGS", 20, 20, 20, WHITE);

    DrawText("Master Volume", 20, 80, 20, DARKGRAY);
    DrawRectangle(20, 110, 200, 10, LIGHTGRAY);
    DrawRectangle(20, 110, 200 * phone->settings.masterVolume, 10, BLUE);
    
    Rectangle minus = { 20, 130, 40, 40 };
    Rectangle plus = { 70, 130, 40, 40 };
    
    if (GuiButton(minus, "-", DARKGRAY, mouse, click)) {
        phone->settings.masterVolume -= 0.1f;
        if (phone->settings.masterVolume < 0) phone->settings.masterVolume = 0;
    }
    if (GuiButton(plus, "+", DARKGRAY, mouse, click)) {
        phone->settings.masterVolume += 0.1f;
        if (phone->settings.masterVolume > 1) phone->settings.masterVolume = 1;
    }

    Rectangle muteBtn = { 20, 200, 100, 40 };
    Color muteColor = phone->settings.mute ? RED : GREEN;
    const char* muteText = phone->settings.mute ? "MUTED" : "SOUND ON";
    
    if (GuiButton(muteBtn, muteText, muteColor, mouse, click)) {
        phone->settings.mute = !phone->settings.mute;
    }
}

// --- MAIN UPDATE LOOP ---
void UpdatePhone(PhoneState *phone, Player *player, GameMap *map) {
    if (IsKeyPressed(KEY_TAB)) phone->isOpen = !phone->isOpen;
    float target = phone->isOpen ? 1.0f : 0.0f;
    phone->slideAnim += (target - phone->slideAnim) * 0.1f;

    if (phone->music.isPlaying) {
        UpdateMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
    }
    
    // --- 1. DELIVERY LOGIC (Detect Arrival) ---
    // This runs every frame to check if player hit the target
    for(int i=0; i<5; i++) {
        DeliveryTask *t = &phone->tasks[i];
        Vector2 playerPos2D = { player->position.x, player->position.z };

        // Logic 1: Picking up Food
        if (t->status == JOB_ACCEPTED) {
            if (Vector2Distance(playerPos2D, t->restaurantPos) < 6.0f) {
                // Arrived at Store!
                t->status = JOB_PICKED_UP;
                
                // Auto-update GPS to customer
                SetMapDestination(map, t->customerPos);
                
                // Optional: Play Sound
                // PlaySound(fxPickup);
                printf("ORDER PICKED UP!\n");
            }
        }
        // Logic 2: Delivering Food
        else if (t->status == JOB_PICKED_UP) {
            if (Vector2Distance(playerPos2D, t->customerPos) < 6.0f) {
                // Arrived at Customer!
                t->status = JOB_DELIVERED;
                
                // Payment
                AddMoney(player, "Delivery Payout", t->pay);
                
                // Clear GPS (handled in UpdateMapsApp proximity check, but force it here too)
                // We don't have direct access to mapState here easily without extern, 
                // but the MapsApp update loop handles clearing the line.
                
                printf("ORDER DELIVERED! +$%.2f\n", t->pay);
            }
        }
    }

    // --- MOUSE CALC ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float phoneX = screenW - PHONE_WIDTH - 50;
    float phoneY = screenH - (PHONE_HEIGHT * phone->slideAnim) + 20;

    Vector2 globalMouse = GetMousePosition();
    Vector2 localMouse = { -1, -1 };
    Rectangle screenDest = { phoneX + SCREEN_OFFSET_X, phoneY + SCREEN_OFFSET_Y, SCREEN_WIDTH, SCREEN_HEIGHT };

    if (CheckCollisionPointRec(globalMouse, screenDest)) {
        localMouse.x = globalMouse.x - screenDest.x;
        localMouse.y = globalMouse.y - screenDest.y;
    }
    
    bool isClicking = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (phone->isOpen) {
        if (phone->currentApp == APP_MAP) {
            UpdateMapsApp(map, (Vector2){player->position.x, player->position.z}, player->angle, localMouse, isClicking);
        }
        
        if (phone->currentApp == APP_HOME && isClicking) {
            int cols = 2;
            float iconSize = 90;
            float gap = 20;
            float startX = (SCREEN_WIDTH - (cols*iconSize + (cols-1)*gap)) / 2;
            float startY = 80;
            Rectangle mapBtn = { startX + 1*(iconSize+gap), startY + 0*(iconSize+gap), iconSize, iconSize };
            
            if (CheckCollisionPointRec(localMouse, mapBtn)) {
                ResetMapCamera((Vector2){player->position.x, player->position.z});
            }
        }
    }
}

void DrawPhone(PhoneState *phone, Player *player, GameMap *map) {
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float phoneX = screenW - PHONE_WIDTH - 50;
    float phoneY = screenH - (PHONE_HEIGHT * phone->slideAnim) + 20;

    Vector2 globalMouse = GetMousePosition();
    Vector2 localMouse = { -1, -1 };
    bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Rectangle screenDest = { phoneX + SCREEN_OFFSET_X, phoneY + SCREEN_OFFSET_Y, SCREEN_WIDTH, SCREEN_HEIGHT };

    if (CheckCollisionPointRec(globalMouse, screenDest)) {
        localMouse.x = globalMouse.x - screenDest.x;
        localMouse.y = globalMouse.y - screenDest.y;
    }

    BeginTextureMode(phone->screenTexture);
        ClearBackground(RAYWHITE);
        DrawRectangle(0, 0, SCREEN_WIDTH, 20, BLACK);
        DrawText("12:00", SCREEN_WIDTH - 40, 2, 10, WHITE);
        
        switch (phone->currentApp) {
            case APP_HOME: DrawAppHome(phone, localMouse, click); break;
            case APP_DELIVERY: DrawAppDelivery(phone, map, localMouse, click); break; 
            case APP_BANK: DrawAppBank(phone, player); break; 
            case APP_MAP: DrawMapsApp(map); break;
            case APP_MUSIC: DrawAppMusic(phone, localMouse, click); break;
            case APP_SETTINGS: DrawAppSettings(phone, localMouse, click); break;
            case APP_BROWSER: 
                DrawText("404 Error", 80, 250, 20, RED); 
                break;
            default: break;
        }

        Rectangle homeBtn = { SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 30, 100, 10 };
        Color homeColor = (CheckCollisionPointRec(localMouse, homeBtn)) ? BLACK : LIGHTGRAY;
        DrawRectangleRec(homeBtn, homeColor);
        
        if (CheckCollisionPointRec(localMouse, homeBtn) && click) {
            phone->currentApp = APP_HOME;
        }
        
    EndTextureMode();

    DrawRectangle(phoneX + 10, phoneY + 10, PHONE_WIDTH, PHONE_HEIGHT, Fade(BLACK, 0.5f));
    DrawRectangleRounded((Rectangle){phoneX, phoneY, PHONE_WIDTH, PHONE_HEIGHT}, 0.1f, 10, (Color){30, 30, 30, 255});
    DrawRectangleLinesEx((Rectangle){phoneX, phoneY, PHONE_WIDTH, PHONE_HEIGHT}, 4, DARKGRAY);
    
    Rectangle src = { 0, 0, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT };
    DrawTexturePro(phone->screenTexture.texture, src, screenDest, (Vector2){0,0}, 0.0f, WHITE);
}

void UnloadPhone(PhoneState *phone) {
    UnloadRenderTexture(phone->screenTexture);
    for(int i=0; i<3; i++) {
        UnloadMusicStream(phone->music.library[i].stream);
    }
}
#include "phone.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h> 

#include "player.h" 
#include "map.h"
#include "maps_app.h" 
#include "delivery_app.h"
#include "car_monitor.h" // [NEW]

// --- CONSTANTS ---
#define BASE_SCREEN_H 720.0f
#define PHONE_SCALE_MOD 0.8f

// --- NOTIFICATION STATE ---
static char notifText[64] = "";
static float notifTimer = 0.0f;
static Color notifColor = WHITE;

// --- Helper Functions ---
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

// --- PUBLIC: NOTIFICATIONS ---
void ShowPhoneNotification(const char *text, Color color) {
    snprintf(notifText, 64, "%s", text);
    notifColor = color;
    notifTimer = 4.0f; 
}

// --- Initialization ---
void InitPhone(PhoneState *phone, GameMap *map) {
    phone->screenTexture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    phone->currentApp = APP_HOME;
    phone->isOpen = false;
    phone->slideAnim = 0.0f;
    
    InitMapsApp(); 
    InitDeliveryApp(phone, map);

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

    phone->settings.masterVolume = 0.8f;
    phone->settings.sfxVolume = 1.0f;
    phone->settings.mute = false;
}

// --- App Draw Functions ---

void DrawAppHome(PhoneState *phone, Player *player, Vector2 mouse, bool click) {
    const char* icons[] = { "Jobs", "Maps", "Bank", "Music", "Settings", "Web", "CarMon" };
    Color colors[] = { ORANGE, BLUE, GREEN, PURPLE, GRAY, SKYBLUE, BLACK };
    
    // Only show CarMon if bought
    int totalApps = player->hasCarMonitorApp ? 7 : 6;

    int cols = 2;
    float iconSize = 90;
    float gap = 20;
    float startX = (SCREEN_WIDTH - (cols*iconSize + (cols-1)*gap)) / 2;
    float startY = 80;

    for (int i = 0; i < totalApps; i++) {
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
            if (i == 6) phone->currentApp = APP_CAR_MONITOR;
        }
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
    
    if (notifTimer > 0) notifTimer -= GetFrameTime();
    UpdateDeliveryApp(phone, player, map);

    // --- SCALED MOUSE CALCULATION ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    
    float scale = (screenH / BASE_SCREEN_H) * PHONE_SCALE_MOD;
    
    float currentPhoneW = PHONE_WIDTH * scale;
    float currentPhoneH = PHONE_HEIGHT * scale;
    float currentOffX = SCREEN_OFFSET_X * scale;
    float currentOffY = SCREEN_OFFSET_Y * scale;

    float phoneX = screenW - currentPhoneW - (50 * scale);
    float phoneY = screenH - (currentPhoneH * phone->slideAnim) + (20 * scale);

    // Render Destination Logic
    Rectangle renderDest = { 
        phoneX + currentOffX, 
        phoneY + currentOffY, 
        (float)SCREEN_WIDTH * scale,  
        (float)SCREEN_HEIGHT * scale 
    };
    
    // Mouse Mapping
    Vector2 globalMouse = GetMousePosition();
    Vector2 localMouse = { -1, -1 };
    
    if (CheckCollisionPointRec(globalMouse, renderDest)) {
        float relX = globalMouse.x - renderDest.x;
        float relY = globalMouse.y - renderDest.y;
        
        float normX = relX / renderDest.width;
        float normY = relY / renderDest.height;
        
        localMouse.x = normX * SCREEN_WIDTH;
        localMouse.y = normY * SCREEN_HEIGHT;
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

void DrawPhone(PhoneState *phone, Player *player, GameMap *map, Vector2 localMouse, bool click) {
    // --- SCALING CALCULATIONS ---
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    
    float scale = (screenH / BASE_SCREEN_H) * PHONE_SCALE_MOD;
    
    float currentPhoneW = PHONE_WIDTH * scale;
    float currentPhoneH = PHONE_HEIGHT * scale;
    
    float phoneX = screenW - currentPhoneW - (50 * scale);
    float phoneY = screenH - (currentPhoneH * phone->slideAnim) + (20 * scale);
    
    Rectangle screenDest = { 
        phoneX + (SCREEN_OFFSET_X * scale), 
        phoneY + (SCREEN_OFFSET_Y * scale), 
        (float)SCREEN_WIDTH * scale, 
        (float)SCREEN_HEIGHT * scale 
    };

    Vector2 globalMouse = GetMousePosition();
    if (CheckCollisionPointRec(globalMouse, screenDest)) {
         localMouse.x = (globalMouse.x - screenDest.x) * (SCREEN_WIDTH / screenDest.width);
         localMouse.y = (globalMouse.y - screenDest.y) * (SCREEN_HEIGHT / screenDest.height);
    }

    BeginTextureMode(phone->screenTexture);
        ClearBackground(RAYWHITE);
        DrawRectangle(0, 0, SCREEN_WIDTH, 20, BLACK);
        DrawText("12:00", SCREEN_WIDTH - 40, 2, 10, WHITE);
        
        switch (phone->currentApp) {
            case APP_HOME: DrawAppHome(phone, player, localMouse, click); break;
            case APP_DELIVERY: DrawDeliveryApp(phone, player, map, localMouse, click); break;
            case APP_BANK: DrawAppBank(phone, player); break; 
            case APP_MAP: DrawMapsApp(map); break;
            case APP_MUSIC: DrawAppMusic(phone, localMouse, click); break;
            case APP_SETTINGS: DrawAppSettings(phone, localMouse, click); break;
            case APP_BROWSER: DrawText("404 Error", 80, 250, 20, RED); break;
            case APP_CAR_MONITOR: DrawCarMonitorApp(player, localMouse, click); break; // [NEW]
            default: break;
        }

        Rectangle homeBtn = { SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 30, 100, 10 };
        Color homeColor = (CheckCollisionPointRec(localMouse, homeBtn)) ? BLACK : LIGHTGRAY;
        DrawRectangleRec(homeBtn, homeColor);
        
        if (CheckCollisionPointRec(localMouse, homeBtn) && click) {
            phone->currentApp = APP_HOME;
        }
        
        // --- DRAW NOTIFICATION BANNER ---
        if (notifTimer > 0) {
            float alpha = (notifTimer > 0.5f) ? 1.0f : (notifTimer * 2.0f);
            Rectangle notifRect = { 10, 30, SCREEN_WIDTH - 20, 50 };
            DrawRectangleRounded(notifRect, 0.2f, 4, Fade(DARKGRAY, 0.95f * alpha));
            DrawRectangleRoundedLines(notifRect, 0.2f, 4, Fade(notifColor, alpha));
            DrawCircle(35, 55, 15, Fade(notifColor, alpha));
            DrawText("NOTIFICATION", 60, 35, 10, Fade(GRAY, alpha));
            DrawText(notifText, 60, 48, 18, Fade(WHITE, alpha));
        }
        
    EndTextureMode();

    DrawRectangle(phoneX + (10*scale), phoneY + (10*scale), currentPhoneW, currentPhoneH, Fade(BLACK, 0.5f)); 
    DrawRectangleRounded((Rectangle){phoneX, phoneY, currentPhoneW, currentPhoneH}, 0.1f, 10, (Color){30, 30, 30, 255});
    DrawRectangleLinesEx((Rectangle){phoneX, phoneY, currentPhoneW, currentPhoneH}, 4 * scale, DARKGRAY);
    
    Rectangle src = { 0, 0, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT };
    DrawTexturePro(phone->screenTexture.texture, src, screenDest, (Vector2){0,0}, 0.0f, WHITE);
}

void UnloadPhone(PhoneState *phone) {
    UnloadRenderTexture(phone->screenTexture);
    for(int i=0; i<3; i++) {
        UnloadMusicStream(phone->music.library[i].stream);
    }
}
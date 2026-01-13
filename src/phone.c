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


#include "phone.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h> 
#include <time.h> 
#include <string.h>
#include "save.h"
#include "player.h" 
#include "map.h"
#include "maps_app.h" 
#include "delivery_app.h"
#include "car_monitor.h" 

// --- CONSTANTS ---
#define BASE_SCREEN_H 720.0f
#define PHONE_SCALE_MOD 0.8f

// --- STATIC ASSETS ---
static Texture2D iconJob;
static Texture2D iconMap;
static Texture2D iconBank;
static Texture2D iconMusic;
static Texture2D iconSettings;
static Texture2D iconCar;

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
    
    // Load App Icons
    iconJob      = LoadTexture("resources/Phoneicons/delivery-truck.png");
    iconMap      = LoadTexture("resources/Phoneicons/treasure-map.png");
    iconBank     = LoadTexture("resources/Phoneicons/atm-card.png");
    iconMusic    = LoadTexture("resources/Phoneicons/music.png");
    iconSettings = LoadTexture("resources/Phoneicons/cogwheel.png");
    iconCar      = LoadTexture("resources/Phoneicons/customization.png");

    SetTextureFilter(iconJob, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconMap, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconBank, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconMusic, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconSettings, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconCar, TEXTURE_FILTER_BILINEAR);
    
    InitMapsApp(); 
    InitDeliveryApp(phone, map);

    // Init Music
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

bool DrawAppIcon(Texture2D icon, const char* label, int shortcutKey, float x, float y, float size, Vector2 mouse, bool click) {
    Rectangle bounds = { x, y, size, size + 25 }; 
    bool hover = CheckCollisionPointRec(mouse, bounds);
    
    float scale = size / (float)icon.width;
    if (hover) scale *= 1.1f; 
    
    float drawX = x + (size - (icon.width * scale)) / 2;
    float drawY = y + (size - (icon.height * scale)) / 2;
    
    DrawTextureEx(icon, (Vector2){drawX, drawY}, 0.0f, scale, WHITE);

    int txtW = MeasureText(label, 20);
    DrawText(label, x + (size - txtW)/2, y + size + 5, 20, BLACK);

    if (shortcutKey > 0) {
        DrawCircle(x + size - 5, y + 5, 12, Fade(BLACK, 0.6f));
        DrawText(TextFormat("%d", shortcutKey), x + size - 8, y - 2, 14, WHITE);
    }
    
    return (hover && click);
}

void DrawAppHome(PhoneState *phone, Player *player, Vector2 mouse, bool click) {
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SKYBLUE, RAYWHITE);
    DrawRectangle(0, 0, SCREEN_WIDTH, 30, Fade(BLACK, 0.2f));

    int cols = 2;
    float iconSize = 80;
    float gapX = 60;
    float gapY = 50;
    
    float totalW = (cols * iconSize) + ((cols - 1) * gapX);
    float startX = (SCREEN_WIDTH - totalW) / 2;
    float startY = 100;

    if (DrawAppIcon(iconJob, "Jobs", 1, startX, startY, iconSize, mouse, click)) {
        phone->currentApp = APP_DELIVERY;
    }
    
    if (DrawAppIcon(iconMap, "Maps", 2, startX + iconSize + gapX, startY, iconSize, mouse, click)) {
        phone->currentApp = APP_MAP;
    }

    if (DrawAppIcon(iconBank, "Bank", 3, startX, startY + iconSize + gapY, iconSize, mouse, click)) {
        phone->currentApp = APP_BANK;
    }

    if (DrawAppIcon(iconMusic, "Music", 4, startX + iconSize + gapX, startY + iconSize + gapY, iconSize, mouse, click)) {
        phone->currentApp = APP_MUSIC;
    }

    if (DrawAppIcon(iconSettings, "Settings", 5, startX, startY + (iconSize + gapY)*2, iconSize, mouse, click)) {
        phone->currentApp = APP_SETTINGS;
    }

    if (player->hasCarMonitorApp) {
        if (DrawAppIcon(iconCar, "CarMon", 6, startX + iconSize + gapX, startY + (iconSize + gapY)*2, iconSize, mouse, click)) {
            phone->currentApp = APP_CAR_MONITOR;
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

// Helper to parse "Artist - Title.mp3" into clean strings
void ParseFilename(const char* filename, char* titleOut, char* artistOut) {
    // 1. Remove extension
    char raw[64];
    strncpy(raw, filename, 63);
    raw[63] = '\0';
    char *dot = strrchr(raw, '.');
    if (dot) *dot = '\0';

    // 2. Check for " - " separator
    char *dash = strstr(raw, " - ");
    if (dash) {
        // Found separator: "Artist - Title"
        *dash = '\0'; // Split string
        strncpy(artistOut, raw, 63);
        strncpy(titleOut, dash + 3, 63); // Skip " - "
    } else {
        // No separator: Use filename as title
        strncpy(titleOut, raw, 63);
        strcpy(artistOut, "Unknown Artist");
    }
}

void LoadMusicLibrary(PhoneState *phone) {
    if (phone->music.isInitialized) return;

    phone->music.songCount = 0;
    phone->music.currentSongIdx = 0;
    
    // 1. Scan Directory
    if (DirectoryExists("resources/Music")) {
        FilePathList files = LoadDirectoryFiles("resources/Music");
        
        for (int i = 0; i < files.count; i++) {
            if (phone->music.songCount >= MAX_SONGS) break;
            
            const char* ext = GetFileExtension(files.paths[i]);
            
            // 2. Filter Valid Audio Files
            if (IsFileExtension(files.paths[i], ".mp3") || 
                IsFileExtension(files.paths[i], ".wav") || 
                IsFileExtension(files.paths[i], ".ogg") || 
                IsFileExtension(files.paths[i], ".qoa")) 
            {
                Song *s = &phone->music.library[phone->music.songCount];
                
                // Load Stream
                s->stream = LoadMusicStream(files.paths[i]);
                s->stream.looping = false; // We handle playlist looping manually
                
                // Get Metadata
                s->duration = GetMusicTimeLength(s->stream);
                ParseFilename(GetFileName(files.paths[i]), s->title, s->artist);
                
                phone->music.songCount++;
            }
        }
        UnloadDirectoryFiles(files);
    }

    if (phone->music.songCount > 0) {
        printf("MUSIC: Loaded %d songs.\n", phone->music.songCount);
    } else {
        printf("MUSIC: No songs found in resources/Music.\n");
    }
    
    phone->music.isInitialized = true;
}

void DrawAppMusic(PhoneState *phone, Vector2 mouse, bool click) {
    // 1. Ensure Library is Loaded
    if (!phone->music.isInitialized) LoadMusicLibrary(phone);

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){20, 20, 30, 255});

    // --- CASE A: NO SONGS FOUND ---
    if (phone->music.songCount == 0) {
        DrawText("No Music Found", 60, 200, 20, RED);
        DrawText("Add .mp3 files to", 50, 230, 10, GRAY);
        DrawText("resources/Music/", 50, 245, 10, GRAY);
        return;
    }

    // --- CASE B: MUSIC PLAYER ---
    Song *s = &phone->music.library[phone->music.currentSongIdx];

    // Update Stream Buffer (MUST be called every frame while playing)
    if (phone->music.isPlaying) {
        UpdateMusicStream(s->stream);
        
        // Auto-Next song when finished
        if (GetMusicTimePlayed(s->stream) >= s->duration - 0.1f) {
            StopMusicStream(s->stream);
            phone->music.currentSongIdx++;
            if (phone->music.currentSongIdx >= phone->music.songCount) phone->music.currentSongIdx = 0;
            PlayMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
        }
    }

    // Album Art Placeholder (Dynamic Color based on Title Hash)
    int colorSeed = 0;
    for(int i=0; s->title[i]; i++) colorSeed += s->title[i];
    Color artColor = (Color){ (colorSeed * 50)%255, (colorSeed * 30)%255, (colorSeed * 90)%255, 255 };
    
    DrawRectangle(40, 60, 200, 200, artColor);
    DrawText("MUSIC", 110, 150, 20, Fade(WHITE, 0.5f)); // Placeholder text

    // Song Info
    DrawText(s->title, 20, 280, 24, WHITE);
    DrawText(s->artist, 20, 310, 18, GRAY);

    // Progress Bar
    float timePlayed = GetMusicTimePlayed(s->stream);
    float progress = 0.0f;
    if (s->duration > 0) progress = timePlayed / s->duration;
    
    float barWidth = SCREEN_WIDTH - 40;
    DrawRectangle(20, 360, barWidth, 4, GRAY);
    DrawRectangle(20, 360, barWidth * progress, 4, GREEN);
    
    // Time Text (Current / Total)
    DrawText(TextFormat("%02d:%02d", (int)timePlayed/60, (int)timePlayed%60), 20, 370, 10, LIGHTGRAY);
    DrawText(TextFormat("%02d:%02d", (int)s->duration/60, (int)s->duration%60), SCREEN_WIDTH - 50, 370, 10, LIGHTGRAY);

    // Controls
    Rectangle btnPrev = { 30, 400, 60, 60 };
    Rectangle btnPlay = { 110, 400, 60, 60 };
    Rectangle btnNext = { 190, 400, 60, 60 };

    // PREV
    if (GuiButton(btnPrev, "|<", DARKBLUE, mouse, click)) {
        StopMusicStream(s->stream); 
        phone->music.isPlaying = true; 
        phone->music.currentSongIdx--;
        // Dynamic Wrap-around
        if(phone->music.currentSongIdx < 0) phone->music.currentSongIdx = phone->music.songCount - 1; 
        PlayMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
    }
    
    // PLAY/PAUSE
    const char* playIcon = phone->music.isPlaying ? "||" : ">";
    Color playColor = phone->music.isPlaying ? GREEN : ORANGE;
    
    if (GuiButton(btnPlay, playIcon, playColor, mouse, click)) {
        phone->music.isPlaying = !phone->music.isPlaying;
        if (phone->music.isPlaying) PlayMusicStream(s->stream); // Use Play instead of Resume for safer state
        else PauseMusicStream(s->stream);
    }

    // NEXT
    if (GuiButton(btnNext, ">|", DARKBLUE, mouse, click)) {
        StopMusicStream(s->stream); 
        phone->music.isPlaying = true; 
        phone->music.currentSongIdx++;
        // Dynamic Wrap-around
        if(phone->music.currentSongIdx >= phone->music.songCount) phone->music.currentSongIdx = 0; 
        PlayMusicStream(phone->music.library[phone->music.currentSongIdx].stream);
    }
    
    // Song Counter (e.g. "1 / 12")
    DrawText(TextFormat("%d / %d", phone->music.currentSongIdx + 1, phone->music.songCount), 120, 470, 10, GRAY);
}

void DrawAppSettings(PhoneState *phone, Player *player, Vector2 mouse, bool click) {
    DrawRectangle(0, 0, SCREEN_WIDTH, 60, GRAY);
    DrawText("SETTINGS", 20, 20, 20, WHITE);
    
    // --- Volume Controls ---
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

    // --- DATA MANAGEMENT ---
    DrawLine(20, 260, SCREEN_WIDTH-20, 260, DARKGRAY);
    DrawText("GAME DATA", 20, 270, 20, DARKGRAY);

    // [NEW] Help Button
    Rectangle helpBtn = { 20, 300, 240, 40 };
    if (GuiButton(helpBtn, "OPEN APP GUIDE", DARKPURPLE, mouse, click)) {
        ShowTutorialHelp(); // Call tutorial helper
    }

    // Moved these down
    Rectangle saveBtn = { 20, 360, 110, 50 };
    Rectangle loadBtn = { 150, 360, 110, 50 };
    Rectangle resetBtn = { 20, 430, 240, 40 };

    if (GuiButton(saveBtn, "SAVE", BLUE, mouse, click)) {
        if (SaveGame(player, phone)) ShowPhoneNotification("Game Saved!", GREEN);
        else ShowPhoneNotification("Save Failed!", RED);
    }

    if (GuiButton(loadBtn, "LOAD", ORANGE, mouse, click)) {
        if (LoadGame(player, phone)) ShowPhoneNotification("Game Loaded!", ORANGE);
        else ShowPhoneNotification("No Save Found", GRAY);
    }

    if (GuiButton(resetBtn, "RESET DATA", RED, mouse, click)) {
        ResetSaveGame(player, phone);
        ShowPhoneNotification("Data Wiped", RED);
    }
}

// --- MAIN UPDATE LOOP ---
void UpdatePhone(PhoneState *phone, Player *player, GameMap *map) {
    if (IsKeyPressed(KEY_TAB)) phone->isOpen = !phone->isOpen;
    float target = phone->isOpen ? 1.0f : 0.0f;
    phone->slideAnim += (target - phone->slideAnim) * 0.1f;

    // --- KEYBOARD SHORTCUTS ---
    if (phone->isOpen) {
        if (IsKeyPressed(KEY_SPACE)) phone->currentApp = APP_HOME;
        if (IsKeyPressed(KEY_ONE))   phone->currentApp = APP_DELIVERY;
        if (IsKeyPressed(KEY_TWO))   phone->currentApp = APP_MAP;
        if (IsKeyPressed(KEY_THREE)) phone->currentApp = APP_BANK;
        if (IsKeyPressed(KEY_FOUR))  phone->currentApp = APP_MUSIC;
        if (IsKeyPressed(KEY_FIVE))  phone->currentApp = APP_SETTINGS;
        if (IsKeyPressed(KEY_SIX) && player->hasCarMonitorApp) phone->currentApp = APP_CAR_MONITOR;
    }

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

    Rectangle renderDest = { 
        phoneX + currentOffX, 
        phoneY + currentOffY, 
        (float)SCREEN_WIDTH * scale,  
        (float)SCREEN_HEIGHT * scale 
    };
    
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
    }
}

void DrawPhone(PhoneState *phone, Player *player, GameMap *map, Vector2 localMouse, bool click) {
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
        
        if (phone->currentApp != APP_HOME) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RAYWHITE);
        }

        // 1. DRAW APPS (Layer 0)
        switch (phone->currentApp) {
            case APP_HOME: DrawAppHome(phone, player, localMouse, click); break;
            case APP_DELIVERY: DrawDeliveryApp(phone, player, map, localMouse, click); break;
            case APP_BANK: DrawAppBank(phone, player); break; 
            case APP_MAP: DrawMapsApp(map); break;
            case APP_MUSIC: DrawAppMusic(phone, localMouse, click); break;
            case APP_SETTINGS: DrawAppSettings(phone, player, localMouse, click); break; 
            case APP_CAR_MONITOR: DrawCarMonitorApp(player, localMouse, click); break; 
            default: break;
        }

        // 2. DRAW STATUS BAR (Layer 1 - Always on Top)
        DrawRectangle(0, 0, SCREEN_WIDTH, 20, Fade(BLACK, 0.4f)); 
        
        time_t t;
        struct tm *tm_info;
        time(&t);
        tm_info = localtime(&t);
        DrawText(TextFormat("%02d:%02d", tm_info->tm_hour, tm_info->tm_min), 10, 4, 10, WHITE);

        int battX = SCREEN_WIDTH - 35;
        DrawText("84%", battX - 30, 4, 10, WHITE); 
        DrawRectangleLines(battX, 5, 20, 10, WHITE);
        DrawRectangle(battX + 20, 7, 2, 6, WHITE);
        DrawRectangle(battX + 2, 7, 14, 6, GREEN);

        // 3. HOME BUTTON (Layer 2)
        Rectangle homeBtn = { SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 30, 100, 10 };
        Color homeColor = (CheckCollisionPointRec(localMouse, homeBtn)) ? BLACK : LIGHTGRAY;
        DrawRectangleRec(homeBtn, homeColor);
        
        if (CheckCollisionPointRec(localMouse, homeBtn) && click) {
            phone->currentApp = APP_HOME;
        }
        
        // 4. NOTIFICATIONS (Layer 3)
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
    
    UnloadTexture(iconJob);
    UnloadTexture(iconMap);
    UnloadTexture(iconBank);
    UnloadTexture(iconMusic);
    UnloadTexture(iconSettings);
    UnloadTexture(iconCar);

    for(int i=0; i<3; i++) {
        UnloadMusicStream(phone->music.library[i].stream);
    }
}
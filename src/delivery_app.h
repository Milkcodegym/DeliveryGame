#ifndef DELIVERY_APP_H
#define DELIVERY_APP_H

#include "raylib.h"
#include "player.h"  // Ensure this is included so Player struct is known
#include "phone.h"   // Ensure PhoneState is known

// --- Function Prototypes ---

// Initialize the app (fills empty jobs)
void InitDeliveryApp(PhoneState *phone, GameMap *map);

// Update logic (Physics, Timers, Earnings)
void UpdateDeliveryApp(PhoneState *phone, Player *player, GameMap *map);

// Draw the UI (Home, Details, Profile)
// ERROR FIX: Added 'Player *player' here to match the .c file
void DrawDeliveryApp(PhoneState *phone, Player *player, GameMap *map, Vector2 mouse, bool click);
void TriggerPickupAnimation(Vector3 itemPos);
void TriggerDropoffAnimation(Vector3 playerPos, Vector3 targetGroundPos);
Vector3 GetSmartDeliveryPos(GameMap *map, Vector3 buildingCenter); 
void UpdateAndDrawPickupEffects(Vector3 playerPos);
void UpdateDeliveryInteraction(PhoneState *phone, Player *player, GameMap *map, float dt);
bool IsInteractionActive(void);
float GetInteractionTimer(void);
#endif
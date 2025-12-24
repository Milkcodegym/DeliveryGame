#include "start_menu.h"

void startmenu(){
    while (true && !WindowShouldClose()){
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("DELIVERY GAME", 20, 20, 40, RED);
            DrawText("Press ENTER to start the game", 120, 220, 20, DARKGREEN);
        EndDrawing();
        if (IsKeyDown(KEY_ENTER)){break;}
    }

}
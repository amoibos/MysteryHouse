#include "general.h"
#include "rules.h"

static const char msg_windows[] = "windows are boarded up except the attic window";

static _Bool in_forest(void) {
    return game.room_pic[game.current_room] == PIC_FOREST;
}

void general_dispatch(void) {
    ON(V_HELP, N_EMPTY) {
        if (in_forest())
            transcribe("if only i could tell if i'd been here before...");
        else
            transcribe("try going in some direction ex:north,south,east,west,up,down");
        return;
    }

    ON(V_WITH, N_GUN) {
        if (held != NO_OBJECT) {
            transcribe(game.gun_was_shot ? "your gun is empty" : "your gun is now empty");
            game.gun_was_shot = 1;
        } else {
            UNHANDLED();
        }
        return;
    }

    SAY(V_GO, N_WINDOW, msg_windows);

    ON(V_U, N_TREE) {
        if (in_forest()) transcribe("there is nothing special");
        else UNHANDLED();
        return;
    }

    SAY(V_LOOK, N_WINDOW, msg_windows);
    ON(V_LOOK, N_NOTE) {
        transcribe(held != NO_OBJECT ? "there is writing on it" : "im not carrying that");
        return;
    }
    ON(V_LOOK, N_ROOM) {
        set_temp_room(0);
        return;
    }
    ON(V_LOOK, N_TREE) {
        if (in_forest()) transcribe("there is nothing special");
        else UNHANDLED();
        return;
    }
    SAY(V_LOOK, N_GUN, "there is one bullet in the gun");   /* BUG kept: always one bullet */

    ON(V_READ, N_NOTE) {
        if (held != NO_OBJECT) set_temp_room(obj_def[held].temp);
        return;
    }

    ON(V_LIGHT, N_MATCHES) {
        if (held != NO_OBJECT) {
            game.match_duration = 4;
            transcribe("ok");
        } else {
            transcribe("you are not carrying it");
        }
        return;
    }
    ON(V_LIGHT, N_CANDLE) {
        if (key_in_inventory(OBJ_MATCHES))
            swap_inventory(OBJ_UNLIT_CANDLE, OBJ_LIT_CANDLE, "ok", "you don't have it");
        else
            transcribe("you have nothing to light it with");
        return;
    }

    ON(V_UNLIGHT, N_CANDLE) {
        if (held != NO_OBJECT) swap_inventory(OBJ_LIT_CANDLE, OBJ_UNLIT_CANDLE, 0, 0);
        else UNHANDLED();
        return;
    }

    ON(V_DRINK, N_WATER) {
        swap_inventory(OBJ_FULL_PITCHER, OBJ_EMPTY_PITCHER,
                       "ok", "i dont understand what you mean");
        return;
    }
    ON(V_POUR, N_WATER) {
        swap_inventory(OBJ_FULL_PITCHER, OBJ_EMPTY_PITCHER,
                       "the pitcher is empty", "i see no water");
        return;
    }

    SAY(V_BREAK, N_WINDOW, msg_windows);

    ON(V_QUIT, N_EMPTY) {
        replay_game();
        return;
    }
    ON(V_FUCK, N_EMPTY) {
        transcribe("if you feel that way i refuse to play with you...");
        quit_game();
        return;
    }
    SAY(V_SLEEP, N_EMPTY, "i feel much more rested now");
    ON(V_SAVE, N_GAME) {
        save_game();
        return;
    }
    ON(V_RESTORE, N_GAME) {
        restore_game();
        return;
    }
    ON(V_INVENTOR, N_EMPTY) {
        list_inventory();
        return;
    }
}

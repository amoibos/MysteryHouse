#include "rooms.h"
#include "rules.h"

/* Translated from rooms.lua.  Quirks marked "BUG" in the original are kept on
   purpose.  Lua tables that can never be reached (room 15 "climb", room 23
   "up", the first "look" table of room 18 which the second one replaces) are
   left out for the same reason. */

static const char msg_door_closed[] = "the door is closed";
static const char msg_is_open[]     = "it is open";
static const char msg_too_heavy[]   = "it is too heavy";
static const char msg_too_high[]    = "it is too high";
static const char msg_which_dir[]   = "which direction?";
static const char msg_no_remove[]   = "it does not remove";
static const char msg_not_carrying[] = "you are not carrying it";
static const char msg_dont_understand[] = "i don't understand you";
static const char msg_with_what[]   = "with what?";
static const char msg_no_unlock[]   = "you have nothing to unlock it with";
static const char msg_not_open[]    = "it is not open";

#define PIC(r)          (game.room_pic[r])

static void get_water(_Bool running) {
    if (running)
        swap_inventory(OBJ_EMPTY_PITCHER, OBJ_FULL_PITCHER,
                       "your pitcher is full", "you have no container");
    else
        transcribe("i see no water");
}

static void fall_from_window(void) {
    transcribe("you fall to earth. luckily you have only minor injuries.");
    transcribe("unfortunately the ambulance driver smashs into a volkwagen. no survivors. you are dead.");
    replay_game();
}

/* 1 front yard */
static const sRule r1_rules[] = {
    GO(V_GO, N_STAIRS, ROOM_PORCH),
    GO(V_GO, N_TREE, ROOM_FOREST_1),
    GO(V_GO, N_U, ROOM_PORCH),
    GO(V_U, N_STAIRS, ROOM_PORCH),
    MSG(V_U, N_FENCE, msg_too_high),
    MSG(V_LOOK, N_FENCE, "it is very high"),
    MSG(V_LOOK, N_TREE, "there is a forest"),
    MSG(V_JUMP, N_FENCE, msg_too_high),
    END_RULES
};

/* 2 porch */
static const sRule r2_rules[] = {
    GO(V_D, N_STAIRS, ROOM_FRONT_YARD),
    GO(V_GO, N_STAIRS, ROOM_FRONT_YARD),
    GO(V_GO, N_D, ROOM_FRONT_YARD),
    END_RULES
};

static void r2(void) {
    ON(V_OPEN, N_DOOR) {
        if (game.front_door_open) {
            transcribe(msg_is_open);
        } else {
            set_room_pic(ROOM_PORCH, PIC_FRONT_DOOR_OPEN);
            game.front_door_open = 1;
        }
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        if (game.front_door_open) {
            set_room_pic(ROOM_PORCH, PIC_FRONT_DOOR_CLOSED);
            game.front_door_open = 0;
        } else {
            transcribe(msg_door_closed);
        }
        return;
    }
    ON(V_GO, N_DOOR) {
        if (game.front_door_open) {
            game.front_door_open = 0;
            game.front_door_locked = 1;
            game.next_room = ROOM_ENTRY_HALL;
            transcribe("the door has been closed and locked");
        } else {
            transcribe(msg_door_closed);
        }
        return;
    }
}

/* 3 entry hall */
static void r3_after(void) {
    if (PIC(ROOM_ENTRY_HALL) != PIC_ENTRYHALL_CROWDED) {
        del_key_from_room(OBJ_TOM, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_SAM, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_SALLY, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_DOCTOR, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_JOE, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_BILL, ROOM_ENTRY_HALL);
        del_key_from_room(OBJ_DAISY, ROOM_ENTRY_HALL);
    }
}

static void r3_go(unsigned char room) {
    if (PIC(ROOM_ENTRY_HALL) == PIC_ENTRYHALL_CROWDED)
        set_room_pic(ROOM_ENTRY_HALL, PIC_ENTRYHALL_CLOSED);
    game.next_room = room;
}

static const sRule r3_rules[] = {
    MSG(V_GO, N_DOOR, msg_which_dir),
    GO(V_U, N_STAIRS, ROOM_JUNCTION),  /* BUG: 'up stairs' does not swap room pic */
    MSG(V_BREAK, N_DOOR, msg_too_heavy),
    END_RULES
};

static void r3(void) {
    ON(V_GO, N_N) {
        if (PIC(ROOM_ENTRY_HALL) == PIC_ENTRYHALL_OPEN) {
            set_room_pic(ROOM_ENTRY_HALL, PIC_ENTRYHALL_CLOSED);
            game.next_room = ROOM_PORCH;
        } else {
            transcribe(msg_door_closed);
        }
        return;
    }
    ON(V_GO, N_S)      { r3_go(ROOM_DINING_ROOM); return; }
    ON(V_GO, N_E)      { r3_go(ROOM_LIBRARY); return; }
    ON(V_GO, N_W)      { r3_go(ROOM_KITCHEN);  return; }
    ON(V_GO, N_U)      { r3_go(ROOM_JUNCTION); return; }
    ON(V_GO, N_STAIRS) { r3_go(ROOM_JUNCTION); return; }
    ON(V_LOOK, N_PERSON) {
        if (PIC(ROOM_ENTRY_HALL) == PIC_ENTRYHALL_CROWDED)
            transcribe("the people were explained at the beginning of the game.");
        return;
    }
    ON(V_UNLOCK, N_DOOR) {
        if (key_in_inventory(OBJ_SKELETON_KEY)) {
            game.front_door_locked = 0;
            transcribe("ok");
        } else {
            transcribe(msg_no_unlock);
        }
        return;
    }
    ON(V_OPEN, N_DOOR) {
        if (game.front_door_locked) transcribe("it is locked");
        else set_room_pic(ROOM_ENTRY_HALL, PIC_ENTRYHALL_OPEN);
        return;
    }
}

/* 4 kitchen */
static void r4_break_wall(void) {
    if (key_in_inventory(OBJ_SLEDGEHAMMER))
        swap_decorations(ROOM_KITCHEN, OBJ_KITCHEN_BRICKS, OBJ_KITCHEN_HOLE,
                         "the bricks break apart leaving a large hole", 0);
    else
        transcribe("you have nothing strong enough");
}

static const sRule r4_rules[] = {
    MSG(V_GO, N_DOOR, msg_which_dir),
    GO(V_GO, N_E, ROOM_ENTRY_HALL),
    MSG(V_GET, N_CABINET, "it's too heavy to lift"),
    END_RULES
};

static void r4(void) {
    ON(V_GO, N_W) {
        if (PIC(ROOM_KITCHEN) == PIC_KITCHEN_OPEN) game.next_room = ROOM_FOREST_1;
        else transcribe(msg_door_closed);
        return;
    }
    ON(V_GO, N_HOLE) {
        if (key_in_room(OBJ_KITCHEN_HOLE)) game.next_room = ROOM_PANTRY;
        else UNHANDLED();
        return;
    }
    ON(V_GET, N_WATER) {
        get_water(game.kitchen_water_running);
        return;
    }
    ON(V_LIGHT, N_STOVE) {
        transcribe("the stove explodes, you are dead");
        replay_game();
        return;
    }
    ON(V_MOVE, N_CABINET) {
        if (game.cabinet_moved) {
            transcribe("it won't move any farther");
        } else {
            game.cabinet_moved = 1;
            set_obj_pos(OBJ_CABINET, 33, 8);
            decorate(ROOM_KITCHEN, OBJ_KITCHEN_BRICKS);
            transcribe("the wall is bricked up behind it");
        }
        return;
    }
    ON(V_OPEN, N_CABINET) {
        set_obj_pic(OBJ_CABINET, PIC_CABINET_OPEN);
        return;
    }
    ON(V_OPEN, N_REFRIGER) {
        if (obj_pic(OBJ_REFRIGERATOR) == PIC_FRIDGE_OPEN) transcribe(msg_is_open);
        else set_obj_pic(OBJ_REFRIGERATOR, PIC_FRIDGE_OPEN);
        return;
    }
    ON(V_OPEN, N_DOOR) {
        if (PIC(ROOM_KITCHEN) == PIC_KITCHEN_OPEN) {
            transcribe(msg_is_open);
        } else {
            set_room_pic(ROOM_KITCHEN, PIC_KITCHEN_OPEN);
            game.kitchen_open = 1;
        }
        return;
    }
    ON(V_CLOSE, N_CABINET) {
        set_obj_pic(OBJ_CABINET, PIC_CABINET_CLOSED);
        set_temp_room(0);
        return;
    }
    ON(V_CLOSE, N_REFRIGER) {
        if (obj_pic(OBJ_REFRIGERATOR) == PIC_FRIDGE_CLOSED) {
            transcribe("it is not open");
        } else {
            set_obj_pic(OBJ_REFRIGERATOR, PIC_FRIDGE_CLOSED);
            set_temp_room(0);
        }
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        if (PIC(ROOM_KITCHEN) == PIC_KITCHEN_CLOSED) {
            transcribe(msg_door_closed);
        } else {
            set_room_pic(ROOM_KITCHEN, PIC_KITCHEN_CLOSED);
            game.kitchen_open = 0;
        }
        return;
    }
    ON(V_LOOK, N_CABINET) {
        look_inside(OBJ_CABINET, PIC_CABINET_OPEN, 1, "it is not open");
        return;
    }
    ON(V_LOOK, N_REFRIGER) {
        look_inside(OBJ_REFRIGERATOR, PIC_FRIDGE_OPEN, 2, msg_door_closed);
        return;
    }
    ON(V_LOOK, N_SINK) {
        if (game.obj_loc[OBJ_BUTTERKNIFE] == (LOC_TEMP | 3))
            transcribe("there is a butterknife here");
        set_temp_room(3);
        return;
    }
    ON(V_FAUCET, N_ON) {
        game.kitchen_water_running = 1;
        transcribe("water is running into the sink");
        return;
    }
    ON(V_FAUCET, N_OFF) {
        game.kitchen_water_running = 0;
        transcribe("ok");
        return;
    }
    ON(V_BREAK, N_WALL)   { r4_break_wall(); return; }
    ON(V_BREAK, N_BRICKS) { r4_break_wall(); return; }
}

/* forest maze: exits north, south, east, west (0: none) */
#define F(n)            ROOM_FOREST_##n
static const unsigned char forest_exits[8][4] = {
    { F(8), F(6), F(2), F(4) },     /* forest 1 */
    { F(1), F(4), F(3), F(5) },     /* forest 2 */
    { F(2), F(3), F(4), F(6) },     /* forest 3 */
    { F(3), F(1), F(5), F(2) },     /* forest 4 */
    { F(4), 0,    F(6), F(2) },     /* forest 5 */
    { F(5), F(3), F(7), F(1) },     /* forest 6 */
    { F(6), 0,    F(8), 0    },     /* forest 7 */
    { F(7), 0,    F(1), 0    },     /* forest 8 */
};
#undef F

static void forest(void) {
    const unsigned char *exits = forest_exits[game.current_room - ROOM_FOREST_1];
    if (exits[0]) TO(V_GO, N_N, exits[0]);
    if (exits[1]) TO(V_GO, N_S, exits[1]);
    if (exits[2]) TO(V_GO, N_E, exits[2]);
    if (exits[3]) TO(V_GO, N_W, exits[3]);
    if (game.current_room != ROOM_FOREST_1) return;

    ON(V_GO, N_U) {
        if (game.kitchen_open) game.next_room = ROOM_KITCHEN;
        else transcribe("the kitchen door is closed");
        return;
    }
    ON(V_OPEN, N_DOOR) {        /* there is no "close" routine, just as original */
        game.kitchen_open = 1;
        set_room_pic(ROOM_KITCHEN, PIC_KITCHEN_OPEN);
        transcribe("the kitchen door is open");
        return;
    }
}

/* 13 pine tree */
static const sRule r13_rules[] = {
    GO(V_GO, N_U, ROOM_TREETOP),
    GO(V_GO, N_D, ROOM_FOREST_5),
    MSG(V_LOOK, N_TREE, "there is nothing special"),
    GO(V_U, N_TREE, ROOM_TREETOP),
    END_RULES
};

/* 14 library */
static const sRule r14_rules[] = {
    GO(V_GO, N_W, ROOM_ENTRY_HALL),
    MSG(V_GO, N_DOOR, msg_which_dir),
    MSG(V_LOOK, N_SHELVES, "there are not many books left"),
    MSG(V_GET, N_BOOKS, msg_no_remove),
    MSG(V_GET, N_COUCH, msg_too_heavy),
    MSG(V_GET, N_TABLE, msg_too_heavy),
    MSG(V_GET, N_CHAIR, msg_too_heavy),
    END_RULES
};

static void r14(void) {
    ON(V_GO, N_E) {
        if (PIC(ROOM_LIBRARY) == PIC_LIBRARY_CLOSED) transcribe(msg_door_closed);
        else game.next_room = ROOM_SIDE_YARD;
        return;
    }
    ON(V_OPEN, N_DOOR) {
        if (PIC(ROOM_LIBRARY) == PIC_LIBRARY_OPEN) {
            transcribe(msg_is_open);
        } else {
            set_room_pic(ROOM_LIBRARY, PIC_LIBRARY_OPEN);
            set_room_pic(ROOM_SIDE_YARD, PIC_SIDEYARD_OPEN);
        }
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        if (PIC(ROOM_LIBRARY) == PIC_LIBRARY_CLOSED) {
            transcribe(msg_door_closed);
        } else {
            set_room_pic(ROOM_LIBRARY, PIC_LIBRARY_CLOSED);
            set_room_pic(ROOM_SIDE_YARD, PIC_SIDEYARD_CLOSED);
        }
        return;
    }
}

/* 15 side yard */
static const sRule r15_rules[] = {
    GO(V_GO, N_S, ROOM_BACK_YARD),
    GO(V_GO, N_W, ROOM_LIBRARY),  /* BUG: 'w' doesn't check for open door */
    MSG(V_U, N_FENCE, msg_too_high),
    END_RULES
};

static void r15(void) {
    ON(V_GO, N_DOOR) {
        if (PIC(ROOM_SIDE_YARD) == PIC_SIDEYARD_CLOSED) transcribe(msg_door_closed);
        else game.next_room = ROOM_LIBRARY;
        return;
    }
    ON(V_OPEN, N_DOOR) {
        if (PIC(ROOM_SIDE_YARD) == PIC_SIDEYARD_OPEN) {
            transcribe(msg_is_open);
        } else {
            set_room_pic(ROOM_SIDE_YARD, PIC_SIDEYARD_OPEN);
            set_room_pic(ROOM_LIBRARY, PIC_LIBRARY_OPEN);
        }
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        if (PIC(ROOM_SIDE_YARD) == PIC_SIDEYARD_CLOSED) {
            transcribe(msg_door_closed);
        } else {
            set_room_pic(ROOM_SIDE_YARD, PIC_SIDEYARD_CLOSED);
            set_room_pic(ROOM_LIBRARY, PIC_LIBRARY_CLOSED);
        }
        return;
    }
}

/* 16 dining room */
static void r16_before(void) {
    command_handled = 1;
    if (key_in_inventory(OBJ_LIT_CANDLE) && !game.rug_on_fire) {
        game.rug_on_fire = 1;
        transcribe("you trip over rug and fall. oh,oh, you started a fire with your candle!");
        decorate(ROOM_DINING_ROOM, OBJ_DINING_ROOM_FIRE);
    } else if (key_in_room(OBJ_DINING_ROOM_FIRE)) {
        /* this is the only action allowed when the room is on fire */
        if (cur_verb == V_POUR && cur_noun == N_WATER && key_in_inventory(OBJ_FULL_PITCHER)) {
            UNHANDLED();
        } else {
            transcribe("the fire is out of control. you are dead");
            replay_game();
        }
    } else {
        UNHANDLED();
    }
}

static const sRule r16_rules[] = {
    MSG(V_GO, N_DOOR, msg_which_dir),
    GO(V_GO, N_N, ROOM_ENTRY_HALL),
    MSG(V_GET, N_RUG, msg_too_heavy),
    MSG(V_GET, N_TABLE, msg_too_heavy),
    MSG(V_GET, N_LAMP, "it is fastened down"),
    MSG(V_LIGHT, N_LAMP, "there is no wick"),
    END_RULES
};

static void r16(void) {
    ON(V_GO, N_S) {
        if (PIC(ROOM_DINING_ROOM) == PIC_DINING_ROOM_CLOSED) transcribe(msg_door_closed);
        else game.next_room = ROOM_BACK_YARD;
        return;
    }
    ON(V_OPEN, N_DOOR) {
        if (PIC(ROOM_DINING_ROOM) == PIC_DINING_ROOM_OPEN) {
            transcribe(msg_is_open);
        } else {
            set_room_pic(ROOM_DINING_ROOM, PIC_DINING_ROOM_OPEN);
            set_room_pic(ROOM_BACK_YARD, PIC_BACKYARD_HOUSE_OPEN);
        }
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        if (PIC(ROOM_DINING_ROOM) == PIC_DINING_ROOM_CLOSED) {
            transcribe(msg_door_closed);
        } else {
            set_room_pic(ROOM_DINING_ROOM, PIC_DINING_ROOM_CLOSED);
            set_room_pic(ROOM_BACK_YARD, PIC_BACKYARD_HOUSE_CLOSED);
        }
        return;
    }
    ON(V_POUR, N_WATER) {
        if (key_in_inventory(OBJ_FULL_PITCHER) && key_in_room(OBJ_DINING_ROOM_FIRE)) {
            swap_decorations(ROOM_DINING_ROOM, OBJ_DINING_ROOM_FIRE, OBJ_DINING_ROOM_HOLE,
                             "the fire is out", 0);
            swap_inventory(OBJ_FULL_PITCHER, OBJ_EMPTY_PITCHER, "the pitcher is empty", 0);
        }
        return;
    }
    ON(V_LOOK, N_HOLE) {                /* BUG: hole accessible at all times */
        set_temp_room(4);
        return;
    }
}

/* 17 back yard */
static const sRule r17_rules[] = {
    GO(V_GO, N_N, ROOM_SIDE_YARD),
    MSG(V_U, N_FENCE, msg_too_high),
    MSG(V_JUMP, N_FENCE, msg_too_high),
    MSG(V_LOOK, N_BODY, "it is sam, the mechanic. he has been hit in the head by a blunt object"),
    MSG(V_GET, N_BODY, msg_too_heavy),
    END_RULES
};

static void r17(void) {
    ON(V_OPEN, N_DOOR) {                /* BUG?: open/closed doors don't tell you */
        set_room_pic(ROOM_BACK_YARD, PIC_BACKYARD_HOUSE_OPEN);
        set_room_pic(ROOM_DINING_ROOM, PIC_DINING_ROOM_OPEN);
        return;
    }
    ON(V_OPEN, N_GATE) {
        set_obj_pic(OBJ_BACKYARD_FENCE, PIC_BACKYARD_FENCE_OPEN);
        return;
    }
    ON(V_CLOSE, N_DOOR) {
        set_room_pic(ROOM_BACK_YARD, PIC_BACKYARD_HOUSE_CLOSED);
        set_room_pic(ROOM_DINING_ROOM, PIC_DINING_ROOM_CLOSED);
        return;
    }
    ON(V_CLOSE, N_GATE) {
        set_obj_pic(OBJ_BACKYARD_FENCE, PIC_BACKYARD_FENCE_CLOSED);
        return;
    }
    ON(V_GO, N_DOOR) {
        if (PIC(ROOM_BACK_YARD) == PIC_BACKYARD_HOUSE_CLOSED) transcribe(msg_door_closed);
        else game.next_room = ROOM_DINING_ROOM;
        return;
    }
    ON(V_GO, N_GATE) {
        if (obj_pic(OBJ_BACKYARD_FENCE) == PIC_BACKYARD_FENCE_CLOSED) transcribe(msg_not_open);
        else game.next_room = ROOM_CEMETERY;
        return;
    }
}

/* 18 cemetary */
static const sRule r18_rules[] = {
    MSG(V_U, N_FENCE, msg_too_high),
    MSG(V_JUMP, N_FENCE, msg_too_high),
    MSG(V_GET, N_GRAVESTO, msg_too_heavy),
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_PERSON, msg_dont_understand),
    MSG(V_GET, N_JOE, msg_dont_understand),
    MSG(V_READ, N_GRAVESTO, "the writing is worn down"),
    MSG(V_LOOK, N_JOE, "it is joe, the gravedigger. he has just finished digging the six graves"),
    MSG(V_LOOK, N_PERSON,
        "it is joe, the gravedigger. he has just finished digging the six graves"),
    MSG(V_LOOK, N_BODY, "it is joe, the gravedigger"),
    MSG(V_TALK, N_JOE, "he doesn't talk much"),
    MSG(V_TALK, N_PERSON, "he doesn't talk much"),
    MSG(V_KILL, N_JOE, msg_with_what),
    MSG(V_KILL, N_PERSON, msg_with_what),
    END_RULES
};

static void r18(void) {
    ON(V_OPEN, N_GATE) {
        if (PIC(ROOM_CEMETERY) == PIC_CEMETARY_OPEN) transcribe(msg_is_open);
        else set_room_pic(ROOM_CEMETERY, PIC_CEMETARY_OPEN);
        return;
    }
    ON(V_CLOSE, N_GATE) {
        if (PIC(ROOM_CEMETERY) == PIC_CEMETARY_CLOSED) transcribe(msg_door_closed);
        else set_room_pic(ROOM_CEMETERY, PIC_CEMETARY_CLOSED);
        return;
    }
    ON(V_GO, N_GATE) {
        if (PIC(ROOM_CEMETERY) == PIC_CEMETARY_CLOSED) transcribe(msg_door_closed);
        else game.next_room = ROOM_BACK_YARD;
        return;
    }
    ON(V_GO, N_GRAVES) {
        if (!game.joe_dead) {
            transcribe("you fall in one and joe buries you. you are dead");
            replay_game();
        } else {
            transcribe("you fall in one and climb out again");
        }
        return;
    }
    ON(V_GET, N_SHOVEL) {
        if (!game.joe_dead) {
            transcribe("joe won't let you");
        } else {                        /* BUG: teleport shovel */
            del_key_everywhere(OBJ_SHOVEL);
            add_key_to_inventory(OBJ_SHOVEL);
        }
        return;
    }
    ON(V_WITH, N_DAGGER) {
        if (key_in_inventory(OBJ_DAGGER)) {
            decorate(ROOM_CEMETERY, OBJ_JOE_XS);   /* BUG: dots do not swap for x's */
            game.joe_dead = 1;
            transcribe("there is a dead body here");
        } else {
            transcribe(msg_not_carrying);
            del_key_from_room(OBJ_JOE_DOTS, ROOM_CEMETERY);    /* BUG: removes joe's eyes! */
        }
        return;
    }
    ON(V_WITH, N_GUN) {
        if (key_in_inventory(OBJ_GUN)) {
            decorate(ROOM_CEMETERY, OBJ_JOE_XS);
            game.joe_dead = 1;
            game.gun_was_shot = 1;
            transcribe("there is a dead body here");
            transcribe("your gun is empty");
        }
        return;
    }
}

/* 19 pantry */
static const sRule r19_rules[] = {
    GO(V_GO, N_D, ROOM_PASSAGEWAY),
    GO(V_GO, N_STAIRS, ROOM_PASSAGEWAY),
    GO(V_U, N_STAIRS, ROOM_PASSAGEWAY),  /* BUG: contradicts image */
    MSG(V_GET, N_FOOD, msg_no_remove),
    MSG(V_GET, N_JARS, msg_no_remove),
    MSG(V_GET, N_BOXES, msg_no_remove),
    END_RULES
};

static void r19(void) {
    ON(V_GO, N_HOLE) {
        if (key_in_room_of(OBJ_KITCHEN_HOLE, ROOM_KITCHEN))
            game.next_room = ROOM_KITCHEN;
        else
            transcribe("you go in the hole but cannot continue and have to return.");
        return;
    }
}

/* 20 north/south passageway */
static const sRule r20_rules[] = {
    GO(V_GO, N_S, ROOM_BASEMENT),
    GO(V_GO, N_U, ROOM_PANTRY),
    GO(V_GO, N_STAIRS, ROOM_PANTRY),
    GO(V_U, N_STAIRS, ROOM_PANTRY),
    END_RULES
};

/* 21 basement */
static const sRule r21_rules[] = {
    GO(V_GO, N_N, ROOM_PASSAGEWAY),
    GO(V_GO, N_U, ROOM_ON_STAIRWAY),
    GO(V_GO, N_HOLE, ROOM_TUNNEL),
    GO(V_GO, N_STAIRS, ROOM_ON_STAIRWAY),
    GO(V_GO, N_DOOR, ROOM_PASSAGEWAY),
    MSG(V_GET, N_TABLE, msg_too_heavy),
    MSG(V_GET, N_ALGAE, msg_dont_understand),
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_DAISY, msg_dont_understand),
    MSG(V_LOOK, N_BODY,
        "it is tom, the plumber. he seems to have been stabbed. there is a daisy in his hand"),
    MSG(V_LOOK, N_TABLE, "there is a skeleton key here"),
    MSG(V_LOOK, N_BRICKS, "it is loose"),
    MSG(V_LOOK, N_JEWELS, "they are jewels, all right!"),
    MSG(V_BREAK, N_WALL, "it is too hard"),
    END_RULES
};

static void r21(void) {
    ON(V_GET, N_BRICKS) {               /* BUG: jewels will teleport to room */
        if (key_in_room(OBJ_BRICKS)) {
            unsigned char was = game.obj_loc[OBJ_BRICKS];
            del_key_everywhere(OBJ_JEWELS);
            del_key_from_inventory(OBJ_JEWELS);
            add_key_to_inventory(OBJ_BRICKS);
            /* swap(self.inventory, ...) then swap(self.decorations, ...);
               both print the same message */
            if (was == (LOC_DROPPED | ROOM_BASEMENT)) add_key_to_room(OBJ_JEWELS, ROOM_BASEMENT);
            else if (was == ROOM_BASEMENT) decorate(ROOM_BASEMENT, OBJ_JEWELS);
            transcribe("you have found the jewels!");
        }
        return;
    }
    ON(V_WIPE, N_ALGAE) {
        if (key_in_inventory(OBJ_TOWEL)) {
            if (key_in_room(OBJ_BRICKS)) {
                transcribe("ok");
            } else {
                transcribe("the wall is exposed. there is a loose brick");
                decorate(ROOM_BASEMENT, OBJ_BRICKS);
            }
        } else {
            transcribe("you have nothing to wipe it with");
        }
        return;
    }
}

/* 22 tunnel */
static const sRule r22_rules[] = {
    GO(V_GO, N_N, ROOM_PINE_TREE),
    GO(V_GO, N_HOLE, ROOM_BASEMENT),
    MSG(V_LOOK, N_TUNNEL, "it is very long"),
    END_RULES
};

/* 23 treetop */
static const sRule r23_rules[] = {
    GO(V_GO, N_D, ROOM_PINE_TREE),
    MSG(V_GO, N_HOUSE, "you had better climb down first"),
    MSG(V_GET, N_TELESCOP, "it is fastened down"),
    END_RULES
};

static void r23(void) {
    ON(V_LOOK, N_TELESCOP) {
        set_temp_room(10);
        transcribe("you are looking through the attic window. you see a trapdoor in the attic ceiling");
        decorate(ROOM_ATTIC, OBJ_TRAPDOOR_CLOSED);
        return;
    }
}

/* 24 junction */
static const sRule r24_rules[] = {
    GO(V_GO, N_N, ROOM_STAIRWAY),
    GO(V_GO, N_E, ROOM_DOORWAY_LARGE_BEDROOM),
    GO(V_GO, N_W, ROOM_DOORWAY_NURSERY),
    GO(V_GO, N_D, ROOM_ENTRY_HALL),
    GO(V_GO, N_STAIRS, ROOM_ENTRY_HALL),
    MSG(V_GO, N_HALL, msg_which_dir),
    MSG(V_GO, N_DOOR, "where?"),
    MSG(V_LOOK, N_DOOR, "where?"),
    MSG(V_LOOK, N_HALL, msg_which_dir),
    END_RULES
};

/* 25 doorway to large bedroom */
static const sRule r25_rules[] = {
    GO(V_GO, N_E, ROOM_DOORWAY_SMALL_BEDROOM),
    GO(V_GO, N_W, ROOM_JUNCTION),
    END_RULES
};

static void r25(void) {
    ON(V_GO, N_DOOR) {
        if (key_in_room_of(OBJ_LINE, ROOM_LARGE_BEDROOM))
            transcribe("a dagger is thrown at you from outside the room. it misses!");
        game.next_room = ROOM_LARGE_BEDROOM;
        return;
    }
}

/* 26 large bedroom */
static const sRule r26_rules[] = {
    MSG(V_GET, N_BED, msg_too_heavy),
    MSG(V_GET, N_TRUNK, msg_too_heavy),
    END_RULES
};

static void r26(void) {
    ON(V_GO, N_DOOR) {
        del_key_from_room(OBJ_LINE, ROOM_LARGE_BEDROOM);
        game.next_room = ROOM_DOORWAY_LARGE_BEDROOM;
        return;
    }
    ON(V_GET, N_DAGGER) {
        del_key_from_room(OBJ_LINE, ROOM_LARGE_BEDROOM);
        UNHANDLED();
        return;
    }
}

/* 27, 29, 31, 34: doorways */
static const sRule r27_rules[] = {
    GO(V_GO, N_W, ROOM_DOORWAY_LARGE_BEDROOM),
    GO(V_GO, N_DOOR, ROOM_SMALL_BEDROOM),
    END_RULES
};

/* 28 small bedroom */
static const sRule r28_rules[] = {
    GO(V_GO, N_DOOR, ROOM_DOORWAY_SMALL_BEDROOM),
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_BED, msg_too_heavy),
    MSG(V_LOOK, N_BODY,
        "it is sally, the seamstress. she has a large lump on her head. there is a blond hair on her dress"),
    END_RULES
};

static const sRule r29_rules[] = {
    GO(V_GO, N_E, ROOM_JUNCTION),
    GO(V_GO, N_W, ROOM_DOORWAY_BOYS_BEDROOM),
    GO(V_GO, N_DOOR, ROOM_NURSERY),
    END_RULES
};

/* 30 nursery */
static const sRule r30_rules[] = {
    GO(V_GO, N_DOOR, ROOM_DOORWAY_NURSERY),
    MSG(V_LOOK, N_BODY, "it is dr. green. it appears he has been stabbed"),
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_CHAIR, msg_too_heavy),
    MSG(V_GET, N_TRUNK, msg_too_heavy),
    MSG(V_GET, N_CRADLE, msg_too_heavy),
    END_RULES
};

static const sRule r31_rules[] = {
    GO(V_GO, N_E, ROOM_DOORWAY_NURSERY),
    GO(V_GO, N_DOOR, ROOM_BOYS_BEDROOM),
    END_RULES
};

/* 32 boys bedroom */
static const sRule r32_rules[] = {
    GO(V_GO, N_DOOR, ROOM_DOORWAY_BOYS_BEDROOM),
    MSG(V_GET, N_BED, msg_too_heavy),
    MSG(V_GET, N_TRUNK, msg_too_heavy),
    END_RULES
};

/* 33 stairway */
static const sRule r33_rules[] = {
    GO(V_GO, N_N, ROOM_DOORWAY_STUDY),
    GO(V_GO, N_S, ROOM_JUNCTION),
    GO(V_GO, N_U, ROOM_ATTIC),
    GO(V_GO, N_STAIRS, ROOM_ATTIC),
    GO(V_U, N_STAIRS, ROOM_ATTIC),
    END_RULES
};

static const sRule r34_rules[] = {
    GO(V_GO, N_S, ROOM_STAIRWAY),
    GO(V_GO, N_DOOR, ROOM_STUDY),
    END_RULES
};

/* 35 study */
static void r35_close_wall(void) {
    set_obj_pos(OBJ_PICTURE, 32, 5);
    set_obj_pos(OBJ_BUTTON, 51, 13);
    set_room_pic(ROOM_STUDY, PIC_STUDY_CLOSED);
}

static const sRule r35_rules[] = {
    GO(V_GO, N_N, ROOM_DOORWAY_STUDY),
    GO(V_GO, N_E, ROOM_BATHROOM),
    MSG(V_GO, N_DOOR, msg_which_dir),
    MSG(V_LOOK, N_PICTURE, "its nice but not exactly my cup of tea. thanx for the look though."),
    MSG(V_GET, N_TABLE, msg_too_heavy),
    MSG(V_GET, N_CHAIR, msg_too_heavy),
    MSG(V_UNSCREW, N_BOLTS, msg_with_what),
    END_RULES
};

static void r35(void) {
    ON(V_GO, N_WALL) {
        if (PIC(ROOM_STUDY) == PIC_STUDY_OPEN) {
            r35_close_wall();
            transcribe("the wall closes behind you with a bang");
            game.next_room = ROOM_CRAWLSPACE;
        } else {
            UNHANDLED();
        }
        return;
    }
    ON(V_GET, N_PICTURE) {
        if (game.picture_loose) {
            decorate(ROOM_STUDY, OBJ_BUTTON);
            transcribe("there is a button on the wall");
            UNHANDLED();
        } else {
            transcribe("it is fastened to the wall with four bolts");
        }
        return;
    }
    ON(V_WITH, N_BUTTERKN) {
        if (key_in_inventory(OBJ_BUTTERKNIFE)) {
            game.picture_loose = 1;
            transcribe("the picture is loose");
        } else {
            transcribe(msg_not_carrying);
        }
        return;
    }
    ON(V_CLOSE, N_WALL) {
        set_room_pic(ROOM_STUDY, PIC_STUDY_CLOSED);
        return;
    }
    ON(V_PRESS, N_BUTTON) {
        /* BUG: button does not need to be revealed first */
        if (PIC(ROOM_STUDY) == PIC_STUDY_CLOSED) {
            set_obj_pos(OBJ_PICTURE, 200, 200);
            set_obj_pos(OBJ_BUTTON, 200, 200);
            set_room_pic(ROOM_STUDY, PIC_STUDY_OPEN);
            transcribe("part of the wall opens");
        } else {
            r35_close_wall();
        }
        return;
    }
}

/* 36 crawlspace */
static const sRule r36_rules[] = {
    GO(V_GO, N_D, ROOM_ON_STAIRWAY),
    GO(V_GO, N_STAIRS, ROOM_ON_STAIRWAY),
    MSG(V_OPEN, N_WALL, "it won't open"),
    MSG(V_BREAK, N_WALL, "it is too hard"),
    END_RULES
};

/* 37 on a stairway */
static const sRule r37_rules[] = {
    GO(V_GO, N_U, ROOM_CRAWLSPACE),
    GO(V_GO, N_D, ROOM_BASEMENT),
    MSG(V_GO, N_STAIRS, msg_which_dir),
    END_RULES
};

/* 38 bathroom */
static const sRule r38_rules[] = {
    GO(V_GO, N_DOOR, ROOM_STUDY),  /* BUG: east to get here, but west won't go back */
    MSG(V_GO, N_TOILET, "thanx. that feels ever so much better"),
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_SHOWER, "thank you. i love to feel clean. thats much better."),
    MSG(V_LOOK, N_BODY, "it is bill, the butcher. he has been strangled with a pair of pantyhose"),
    MSG(V_FLUSH, N_TOILET, "ok"),
    END_RULES
};

static void r38(void) {
    ON(V_FAUCET, N_ON) {
        transcribe("water is running into the sink");
        game.bathroom_water_running = 1;
        return;
    }
    ON(V_FAUCET, N_OFF) {
        transcribe("ok");
        game.bathroom_water_running = 0;
        return;
    }
    ON(V_GET, N_WATER) {
        get_water(game.bathroom_water_running);
        return;
    }
}

/* 39 attic */
static const sRule r39_rules[] = {
    GO(V_GO, N_D, ROOM_STAIRWAY),
    GO(V_GO, N_STAIRS, ROOM_STAIRWAY),
    GO(V_GO, N_DOOR, ROOM_STORAGE_ROOM),
    MSG(V_LOOK, N_WINDOW, "you see a forest"),
    MSG(V_GET, N_LADDER, msg_no_remove),
    MSG(V_BREAK, N_WINDOW, "ok"),
    END_RULES
};

static void r39(void) {
    ON(V_GO, N_WINDOW) {
        fall_from_window();
        return;
    }
    ON(V_GO, N_TRAPDOOR) {
        if (key_in_room(OBJ_TRAPDOOR_OPEN)) game.next_room = ROOM_TOWER;
        else if (key_in_room(OBJ_TRAPDOOR_CLOSED)) transcribe(msg_not_open);
        else transcribe("what trapdoor?");
        return;
    }
    ON(V_U, N_LADDER) {
        if (key_in_room(OBJ_TRAPDOOR_OPEN)) game.next_room = ROOM_TOWER;
        else transcribe("you climb up bump your head on the ceiling and fall, dazed but alive.");
        return;
    }
    ON(V_OPEN, N_TRAPDOOR) {
        if (key_in_room(OBJ_TRAPDOOR_OPEN)) {
            transcribe(msg_is_open);
        } else if (key_in_room(OBJ_TRAPDOOR_CLOSED)) {
            del_key_from_room(OBJ_TRAPDOOR_CLOSED, ROOM_ATTIC);
            decorate(ROOM_ATTIC, OBJ_TRAPDOOR_OPEN);
        } else {
            transcribe("what trapdoor?");
        }
        return;
    }
    ON(V_CLOSE, N_TRAPDOOR) {
        if (key_in_room(OBJ_TRAPDOOR_OPEN)) {
            del_key_from_room(OBJ_TRAPDOOR_OPEN, ROOM_ATTIC);
            decorate(ROOM_ATTIC, OBJ_TRAPDOOR_CLOSED);
        } else if (key_in_room(OBJ_TRAPDOOR_CLOSED)) {
            transcribe(msg_door_closed);
        } else {
            transcribe("what trapdoor?");
        }
        return;
    }
    ON(V_HELP, N_EMPTY) {
        if (key_in_room(OBJ_SLEDGEHAMMER)) transcribe("i think i see a sledgehammer here.");
        else UNHANDLED();
        return;
    }
}

/* 40 storage room */
static const sRule r40_rules[] = {
    GO(V_GO, N_DOOR, ROOM_ATTIC),
    MSG(V_GET, N_BOXES, msg_no_remove),
    MSG(V_GET, N_TRUNK, msg_too_heavy),
    END_RULES
};

static void r40(void) {
    ON(V_GO, N_WINDOW) {
        fall_from_window();
        return;
    }
    ON(V_OPEN, N_TRUNK) {
        if (game.trunk_unlocked)        /* BUG: original has broken logic */
            swap_decorations(ROOM_STORAGE_ROOM, OBJ_TRUNK_CLOSED, OBJ_TRUNK_OPEN,
                             "", "it is locked");
        else
            transcribe("it is locked");
        return;
    }
    ON(V_CLOSE, N_TRUNK) {
        swap_decorations(ROOM_STORAGE_ROOM, OBJ_TRUNK_OPEN, OBJ_TRUNK_CLOSED, 0, msg_not_open);
        return;
    }
    ON(V_LOOK, N_TRUNK) {
        if (game.trunk_unlocked) {
            if (key_in_room(OBJ_TRUNK_OPEN)) set_temp_room(5);
            else transcribe(msg_not_open);
        } else {
            transcribe("it is locked");
        }
        return;
    }
    ON(V_UNLOCK, N_TRUNK) {
        if (game.trunk_unlocked) {
            if (key_in_room(OBJ_TRUNK_CLOSED)) transcribe("it is unlocked");
            else UNHANDLED();
        } else if (key_in_inventory(OBJ_SMALL_KEY)) {
            transcribe("ok");
            game.trunk_unlocked = 1;
        } else {
            transcribe(msg_no_unlock);
        }
        return;
    }
}

/* 41 tower */
static void r41_look_daisy(void) {
    transcribe(game.daisy_dead ? "she is dead" : "she is going to kill you");
}

static void r41_with_default(const char *msg) {     /* BUG: daisy_dead not checked */
    if (!msg) msg = msg_not_carrying;
    if (noun_in_inventory(cur_noun) == NO_OBJECT) transcribe(msg);
    transcribe("daisy stabbed you. you are dead");
    replay_game();
}

static const sRule r41_rules[] = {
    GO(V_GO, N_TRAPDOOR, ROOM_ATTIC),
    MSG(V_LOOK, N_BODY, "she is dead"),  /* BUG: daisy_dead not checked */
    MSG(V_KILL, N_DAISY, msg_with_what),  /* BUG: daisy_dead not checked */
    MSG(V_GET, N_BODY, msg_too_heavy),
    MSG(V_GET, N_KNIFE, msg_dont_understand),
    MSG(V_CLOSE, N_TRAPDOOR, "it wont close here"),
    END_RULES
};

static void r41(void) {
    ON(V_LOOK, N_DAISY)  { r41_look_daisy(); return; }
    ON(V_LOOK, N_PERSON) { r41_look_daisy(); return; }
    ON(V_WITH, N_GUN) {
        if (game.daisy_dead) {
            transcribe("daisy is already dead");
        } else if (key_in_inventory(OBJ_GUN)) {
            if (game.gun_was_shot) {
                r41_with_default("your gun is empty");
            } else {
                transcribe("your gun is empty");
                game.gun_was_shot = 1;
                game.daisy_dead = 1;
                swap_decorations(ROOM_TOWER, OBJ_DAISY_DOTS, OBJ_DAISY_XS, "daisy is now dead", 0);
            }
        } else {
            r41_with_default(0);
        }
        return;
    }
    ON(V_WITH, N_DAGGER) {
        if (game.daisy_dead) transcribe("daisy is already dead");
        else r41_with_default(0);
        return;
    }
    ON(V_WITH, N_BUTTERKN) { r41_with_default(0); return; }
    ON(V_WITH, N_SLEDGEHA) { r41_with_default(0); return; }
    ON(V_GET, N_NOTE) {
        if (!game.daisy_dead) {
            transcribe("daisy won't let you");
        } else {
            transcribe("ok");
            UNHANDLED();
        }
        return;
    }
}

typedef struct {
    const sRule *rules;
    void (*fn)(void);
} sRoom;

static const sRoom room_handler[NUM_ROOMS + 1] = {
    { 0, 0 },
    { r1_rules, 0 },
    { r2_rules, r2 },
    { r3_rules, r3 },
    { r4_rules, r4 },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { 0, forest },
    { r13_rules, 0 },
    { r14_rules, r14 },
    { r15_rules, r15 },
    { r16_rules, r16 },
    { r17_rules, r17 },
    { r18_rules, r18 },
    { r19_rules, r19 },
    { r20_rules, 0 },
    { r21_rules, r21 },
    { r22_rules, 0 },
    { r23_rules, r23 },
    { r24_rules, 0 },
    { r25_rules, r25 },
    { r26_rules, r26 },
    { r27_rules, 0 },
    { r28_rules, 0 },
    { r29_rules, 0 },
    { r30_rules, 0 },
    { r31_rules, 0 },
    { r32_rules, 0 },
    { r33_rules, 0 },
    { r34_rules, 0 },
    { r35_rules, r35 },
    { r36_rules, 0 },
    { r37_rules, 0 },
    { r38_rules, r38 },
    { r39_rules, r39 },
    { r40_rules, r40 },
    { r41_rules, r41 },
};

void room_before(void) {
    if (game.current_room == ROOM_DINING_ROOM) r16_before();
}

void room_dispatch(void) {
    const sRoom *r;
    if (!game.current_room || game.current_room > NUM_ROOMS) return;
    r = &room_handler[game.current_room];
    if (r->rules && run_rules(r->rules)) return;
    if (r->fn) r->fn();
}

void room_after(void) {
    if (game.current_room == ROOM_ENTRY_HALL) r3_after();
}

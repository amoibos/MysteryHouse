#include "state.h"

sGame game;

_Bool show_image;
_Bool blank_image;
_Bool prompt_replay;
_Bool view_dirty;

static const unsigned char mutable_obj[NUM_MUTABLE] = {
    OBJ_CABINET, OBJ_REFRIGERATOR, OBJ_BACKYARD_FENCE, OBJ_PICTURE, OBJ_BUTTON
};

static signed char mutable_index(unsigned char obj) {
    signed char i;
    for (i = 0; i < NUM_MUTABLE; ++i)
        if (mutable_obj[i] == obj) return i;
    return -1;
}

/* start_game() + init_objects() + init_rooms() */
void game_init(void) {
    unsigned char i;
    unsigned char *p = (unsigned char *)&game;

    for (i = 0; i < sizeof(game); ++i)
        p[i] = 0;

    for (i = 0; i < NUM_OBJECTS; ++i)
        game.obj_loc[i] = obj_def[i].room;
    for (i = 1; i <= NUM_TEMPS; ++i)
        if (temp_initial_obj[i] != NO_OBJECT)
            game.obj_loc[temp_initial_obj[i]] = LOC_TEMP | i;
    for (i = 0; i <= NUM_ROOMS; ++i)
        game.room_pic[i] = room_start_pic[i];
    for (i = 0; i < NUM_MUTABLE; ++i) {
        const sObjectDef *d = &obj_def[mutable_obj[i]];
        game.mut[i].pic = d->pic;
        game.mut[i].x   = d->x;
        game.mut[i].y   = d->y;
    }

    game.next_room  = ROOM_FRONT_YARD;
    game.turn_count = -1;   /* one extra "turn" runs on game start */

    blank_image   = 0;
    prompt_replay = 0;
    view_dirty    = 1;
}

unsigned char obj_pic(unsigned char obj) {
    signed char m = mutable_index(obj);
    return (m < 0) ? obj_def[obj].pic : game.mut[m].pic;
}

unsigned char obj_x(unsigned char obj) {
    signed char m = mutable_index(obj);
    return (m < 0) ? obj_def[obj].x : game.mut[m].x;
}

unsigned char obj_y(unsigned char obj) {
    signed char m = mutable_index(obj);
    return (m < 0) ? obj_def[obj].y : game.mut[m].y;
}

void set_obj_pic(unsigned char obj, unsigned char pic) {
    game.mut[mutable_index(obj)].pic = pic;
    view_dirty = 1;
}

void set_obj_pos(unsigned char obj, unsigned char x, unsigned char y) {
    signed char m = mutable_index(obj);
    game.mut[m].x = x;
    game.mut[m].y = y;
    view_dirty = 1;
}

void set_room_pic(unsigned char room, unsigned char pic) {
    game.room_pic[room] = pic;
    view_dirty = 1;
}

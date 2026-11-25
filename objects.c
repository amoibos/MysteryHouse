#include "objects.h"
#include "textio.h"

#define TEMP_LOC()      (LOC_TEMP | game.temp_room)

_Bool key_in_inventory(unsigned char obj) {
    return game.obj_loc[obj] == LOC_INV;
}

/* temps[temp_room].decorations, then room.inventory, then room.decorations
   (the latter only while no temp room is shown) */
_Bool key_in_room_of(unsigned char obj, unsigned char room) {
    unsigned char loc = game.obj_loc[obj];
    if (game.temp_room && loc == TEMP_LOC()) return 1;
    if (loc == (LOC_DROPPED | room)) return 1;
    return !game.temp_room && loc == room;
}

_Bool key_in_room(unsigned char obj) {
    return key_in_room_of(obj, game.current_room);
}

static unsigned char find_noun_at(unsigned char noun, unsigned char loc) {
    unsigned char i;
    for (i = 0; i < NUM_OBJECTS; ++i)
        if (game.obj_loc[i] == loc && obj_def[i].noun == noun)
            return i;
    return NO_OBJECT;
}

unsigned char noun_in_inventory(unsigned char noun) {
    return find_noun_at(noun, LOC_INV);
}

unsigned char noun_in_room(unsigned char noun) {
    unsigned char found = NO_OBJECT;
    if (game.temp_room)
        found = find_noun_at(noun, TEMP_LOC());
    if (found == NO_OBJECT)
        found = find_noun_at(noun, LOC_DROPPED | game.current_room);
    if (!game.temp_room && found == NO_OBJECT)
        found = find_noun_at(noun, game.current_room);
    return found;
}

static void set_loc(unsigned char obj, unsigned char loc) {
    game.obj_loc[obj] = loc;
    view_dirty = 1;
}

void decorate(unsigned char room, unsigned char obj) {
    set_loc(obj, room);
}

void add_key_to_inventory(unsigned char obj) {
    set_loc(obj, LOC_INV);
}

void add_key_to_room(unsigned char obj, unsigned char room) {
    set_loc(obj, LOC_DROPPED | room);
}

void del_key_from_inventory(unsigned char obj) {
    if (game.obj_loc[obj] == LOC_INV) set_loc(obj, LOC_NONE);
}

void del_key_from_room(unsigned char obj, unsigned char room) {
    unsigned char loc = game.obj_loc[obj];
    if ((game.temp_room && loc == TEMP_LOC()) ||
        loc == room || loc == (LOC_DROPPED | room))
        set_loc(obj, LOC_NONE);
}

void del_key_everywhere(unsigned char obj) {
    if (game.obj_loc[obj] != LOC_INV) set_loc(obj, LOC_NONE);
}

static void do_swap(unsigned char loc, unsigned char old, unsigned char new_obj,
                    const char *success, const char *failure) {
    if (game.obj_loc[old] == loc) {
        set_loc(old, LOC_NONE);
        set_loc(new_obj, loc);
        if (success) transcribe(success);
    } else {
        if (!failure) failure = success;
        if (failure) transcribe(failure);
    }
}

void swap_inventory(unsigned char old, unsigned char new_obj,
                    const char *success, const char *failure) {
    do_swap(LOC_INV, old, new_obj, success, failure);
}

void swap_decorations(unsigned char room, unsigned char old, unsigned char new_obj,
                      const char *success, const char *failure) {
    do_swap(room, old, new_obj, success, failure);
}

void look_inside(unsigned char obj, unsigned char valid_pic, unsigned char temp,
                 const char *failure) {
    if (obj_pic(obj) == valid_pic)
        set_temp_room(temp);
    else
        transcribe(failure);
}

void set_temp_room(unsigned char temp) {
    if (game.temp_room != temp) view_dirty = 1;
    game.temp_room = temp;
}

void list_inventory(void) {
    unsigned char i;
    for (i = 0; i < NUM_OBJECTS; ++i)
        if (game.obj_loc[i] == LOC_INV)
            transcribe(obj_descr[i]);
}

void replay_game(void) {
    prompt_replay = 1;
}

void quit_game(void) {
    transcribe("thank you for playing hi-res adventure ... good-bye");
    text_wait_and_reset();
}

/* Pico-8 saves to cartdata.  The SD-1000 has no battery backed RAM, so the
   saved game lives in work RAM until the console is switched off. */
static sGame saved_game;
static _Bool have_saved_game;

void save_game(void) {
    saved_game = game;
    have_saved_game = 1;
}

void restore_game(void) {
    if (!have_saved_game) {
        transcribe("there is no saved game");
        return;
    }
    game = saved_game;
    blank_image = 0;
    view_dirty = 1;
}

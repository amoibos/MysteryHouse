#ifndef STATE_H
#define STATE_H

#include "gamedata.h"

/* -- Object locations --------------------------------------------------------
   The Pico-8 version keeps several key sets (inventory, room.decorations,
   room.inventory, temps[t].decorations).  Every object lives in at most one
   of them, so one byte per object is enough:                               */
#define LOC_NONE        0x00            /* removed from play                */
                                        /* 1..41: room decoration (drawn at
                                           the object's own position)       */
#define LOC_DROPPED     0x80            /* | room: room inventory, drawn at
                                           drop positions                   */
#define LOC_TEMP        0x40            /* | temp: temp room decoration     */
#define LOC_INV         0xFF            /* carried by the player            */

#define NO_OBJECT       0xFF

/* objects whose picture / draw position changes during the game */
enum { MUT_CABINET, MUT_REFRIGERATOR, MUT_BACKYARD_FENCE, MUT_PICTURE, MUT_BUTTON,
       NUM_MUTABLE };

typedef struct {
    unsigned char pic, x, y;
} sMutable;

typedef struct {
    /* var_* game conditions */
    _Bool front_door_open;
    _Bool front_door_locked;
    _Bool darkness_arrived;
    _Bool gun_was_shot;
    _Bool cabinet_moved;
    _Bool kitchen_open;
    _Bool kitchen_water_running;
    _Bool joe_dead;
    _Bool picture_loose;
    _Bool bathroom_water_running;
    _Bool trunk_unlocked;
    _Bool daisy_dead;
    _Bool rug_on_fire;

    unsigned char match_duration;
    int           turn_count;

    unsigned char current_room;
    unsigned char next_room;
    unsigned char temp_room;

    unsigned char obj_loc[NUM_OBJECTS];
    unsigned char room_pic[NUM_ROOMS + 1];
    sMutable      mut[NUM_MUTABLE];
} sGame;

extern sGame game;

/* interpreter state, not part of a saved game */
extern _Bool show_image;        /* empty input toggles the picture */
extern _Bool blank_image;       /* darkness */
extern _Bool prompt_replay;     /* "would you like to play again?" pending */
extern _Bool view_dirty;        /* picture must be redrawn */

void game_init(void);
unsigned char obj_pic(unsigned char obj);
unsigned char obj_x(unsigned char obj);
unsigned char obj_y(unsigned char obj);
void set_obj_pic(unsigned char obj, unsigned char pic);
void set_obj_pos(unsigned char obj, unsigned char x, unsigned char y);
void set_room_pic(unsigned char room, unsigned char pic);

#endif

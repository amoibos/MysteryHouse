#include "views.h"
#include "state.h"
#include "pictures.h"
#include "gfx.h"
#include "textio.h"

#define IMAGE_ROWS      (IMAGE_H / 8)

static unsigned char last_room, last_temp;
static _Bool last_blank, last_show = 1;

/* Pico-8 draws the back yard as house picture + fence object; the Apple
   original has one picture per door/gate combination */
static void draw_room_backdrop(void) {
    unsigned char pic = game.room_pic[game.current_room];
    if (game.temp_room) {
        pic = temp_pic[game.temp_room];
    } else if (game.current_room == ROOM_BACK_YARD) {
        pic = PIC_BACKYARD_FIRST + (pic == PIC_BACKYARD_HOUSE_OPEN ? 2 : 0) +
              (obj_pic(OBJ_BACKYARD_FENCE) == PIC_BACKYARD_FENCE_OPEN);
    }
    draw_image(pic, 0, 0);
}

static void draw_room_decorations(void) {
    unsigned char loc = game.temp_room ? (LOC_TEMP | game.temp_room) : game.current_room;
    unsigned char i;
    for (i = 0; i < NUM_OBJECTS; ++i)
        if (game.obj_loc[i] == loc)
            draw_image(obj_pic(i), obj_x(i), obj_y(i));
}

/* "this is dumb, but its how the original works" */
static void draw_room_inventory(void) {
    unsigned char i, j = 0;
    if (game.temp_room) return;
    for (i = 0; i < NUM_OBJECTS && j < NUM_DROP_POS; ++i) {
        if (game.obj_loc[i] == (LOC_DROPPED | game.current_room)) {
            draw_image(obj_pic(i), drop_pos[j][0], drop_pos[j][1]);
            ++j;
        }
    }
}

void view_update(void) {
    if (show_image != last_show) {
        last_show = show_image;
        gfx_clear_rows(0, TEXT_ROW_FIRST);
        text_refresh();
        view_dirty = 1;
    }
    if (!show_image) return;

    if (!view_dirty && last_room == game.current_room && last_temp == game.temp_room &&
        last_blank == blank_image)
        return;
    view_dirty = 0;
    last_room = game.current_room;
    last_temp = game.temp_room;
    last_blank = blank_image;

    gfx_clear_rows(0, IMAGE_ROWS);
    if (blank_image) return;
    draw_room_backdrop();
    draw_room_decorations();
    draw_room_inventory();
}

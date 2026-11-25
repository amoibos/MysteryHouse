#ifndef OBJECTS_H
#define OBJECTS_H

#include "state.h"

/* Object helpers, named after their mh.p8 counterparts.  "room" arguments
   are room numbers; the temp-room rules of key_in_room() / noun_in_room()
   are kept exactly (a temp room hides the room decorations). */

_Bool key_in_inventory(unsigned char obj);
_Bool key_in_room(unsigned char obj);
_Bool key_in_room_of(unsigned char obj, unsigned char room);
unsigned char noun_in_inventory(unsigned char noun);    /* NO_OBJECT if none */
unsigned char noun_in_room(unsigned char noun);

void decorate(unsigned char room, unsigned char obj);
void add_key_to_inventory(unsigned char obj);
void add_key_to_room(unsigned char obj, unsigned char room);
void del_key_from_inventory(unsigned char obj);
void del_key_from_room(unsigned char obj, unsigned char room);
void del_key_everywhere(unsigned char obj);             /* for r in all(rooms) */

/* swap(tabl, old, new, success, failure); failure defaults to success */
void swap_inventory(unsigned char old, unsigned char new_obj,
                    const char *success, const char *failure);
void swap_decorations(unsigned char room, unsigned char old, unsigned char new_obj,
                      const char *success, const char *failure);

void look_inside(unsigned char obj, unsigned char valid_pic, unsigned char temp,
                 const char *failure);
void set_temp_room(unsigned char temp);

void list_inventory(void);
void replay_game(void);
void quit_game(void);
void save_game(void);
void restore_game(void);

#endif

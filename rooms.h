#ifndef ROOMS_H
#define ROOMS_H

/* rooms.lua: per room verb/noun handlers plus the before/after hooks */
void room_before(void);
void room_dispatch(void);
void room_after(void);

#endif

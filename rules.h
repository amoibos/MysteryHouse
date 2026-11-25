#ifndef RULES_H
#define RULES_H

/* Helpers for the verb/noun handler tables of rooms.c and general.c.

   In rooms.lua every room is a table room[verb][noun] whose entries are a
   string (print it), a number (go to that room) or a function.  Finding an
   entry marks the command as handled; a function may clear the flag again
   to let the next stage try.  ON() reproduces that lookup; the plain string
   and number entries are kept as sRule tables (MSG / GO) and looked up by
   run_rules() before the room function runs. */

#include "state.h"
#include "objects.h"
#include "textio.h"

extern unsigned char cur_verb;
extern unsigned char cur_noun;
extern unsigned char held;              /* noun_in_inventory(cur_noun) */
extern _Bool command_handled;

typedef struct {
    unsigned char verb, noun;
    unsigned char room;                 /* GO: next room */
    const char *msg;                    /* MSG: text, 0 for GO */
} sRule;

#define MSG(v, n, s)    { v, n, 0, s }
#define GO(v, n, r)     { v, n, r, 0 }
#define END_RULES       { V_NONE }

/* cur_verb == v && cur_noun == n, then the command is handled */
_Bool on(unsigned char v, unsigned char n);
/* first matching rule of a table ended by END_RULES; 1 if one matched */
_Bool run_rules(const sRule *rule);

#define ON(v, n)        if (on(v, n))
#define SAY(v, n, s)    ON(v, n) { transcribe(s); return; }
#define TO(v, n, r)     ON(v, n) { game.next_room = (r); return; }
#define UNHANDLED()     (command_handled = 0)

#endif

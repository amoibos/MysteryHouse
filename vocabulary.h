#ifndef VOCABULARY_H
#define VOCABULARY_H

#include "gamedata.h"

/* Look up a word (already cut to at most 8 characters, lower case).
   Returns the V_* / N_* id of its primary word, or V_NONE / N_NONE.
   The empty string is a valid noun (N_EMPTY), as in the original. */
unsigned char parse_verb(const char *word);
unsigned char parse_noun(const char *word);

/* the primary verbs that become "go <direction>" when used alone */
_Bool is_direction(unsigned char verb);
unsigned char direction_noun(unsigned char verb);

#endif

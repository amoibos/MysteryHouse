#ifndef TEXTIO_H
#define TEXTIO_H

#include "gamedata.h"

/* Text output and keyboard input.

   With the picture shown, rows 20-22 hold the last transcript lines and
   row 23 is the input line (the Apple II layout: 160 px image + 4 lines).
   Without the picture (empty input toggles it) the whole screen shows the
   transcript.  Lines that would scroll out unseen are paged ("-more-"). */

#define TEXT_COLS           40      /* 6 pixel font */
#define TEXT_ROWS           24
#define TEXT_ROW_FIRST      20      /* first text row below the picture */

void text_keyboard_irq(void);    /* call once per frame (VBlank interrupt) */
void text_clear_history(void);

/* add a message to the transcript (word wrapped like compose_lines()) */
void transcribe(const char *msg);

/* show prompt + read one line; the line is added to the transcript */
const char *text_input(const char *prompt);

/* redraw the text area (after the screen layout changed) */
void text_refresh(void);

/* full screen text page, wait for a key */
void text_page(const char *text);

void text_wait_key(void);
void text_wait_and_reset(void);    /* after "good-bye": a key, then restart */

#endif

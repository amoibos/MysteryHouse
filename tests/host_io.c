/* Host (PC) replacement for textio.c / views.c: runs the game logic with
   stdin/stdout so walkthroughs can be checked without an emulator.
   build: see tests/run_tests.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../textio.h"
#include "../views.h"
#include "../parser.h"
#include "../state.h"
#include "../bpe.h"

static char line[64];

void text_clear_history(void) {}
void text_refresh(void) {}
void text_page(const char *t) { (void)t; }
void text_wait_key(void) {}

void transcribe(const char *msg) {
    msg = bpe_text(msg);
    if (msg && *msg) printf("  %s\n", msg);
}

const char *text_input(const char *prompt) {
    size_t n;
    if (!fgets(line, sizeof line, stdin)) { printf("[eof]\n"); exit(2); }
    n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    printf("%s%s\n", prompt, line);
    return line;
}

void text_wait_and_reset(void) { printf("[halt]\n"); exit(0); }

void view_update(void) {
    unsigned char i;
    if (!view_dirty) return;
    view_dirty = 0;
    printf("    [room %d pic %d temp %d%s; objs:", game.current_room,
           game.room_pic[game.current_room], game.temp_room, blank_image ? " dark" : "");
    for (i = 0; i < NUM_OBJECTS; ++i) {
        unsigned char l = game.obj_loc[i];
        if (l == game.current_room || l == (LOC_DROPPED | game.current_room) ||
            (game.temp_room && l == (LOC_TEMP | game.temp_room)))
            printf(" %d", i);
    }
    printf("]\n");
}


int main(void) {
    begin_game();
    for (;;) game_turn();
}

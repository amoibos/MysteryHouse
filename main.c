#include "SGlib/SGlib.h"
#include "state.h"
#include "parser.h"
#include "views.h"
#include "pictures.h"
#include "textio.h"
#include "gfx.h"
#include "psg/PSGlib.h"

extern const unsigned char title_music[];   /* assets/music.c */

__sfr __at 0x7F psg_port;

/* the PSG is not silent after power on: the channels the music does not
   use yet (noise until the first drum) would play at full volume */
static void psg_silence(void) {
    psg_port = 0x9F;            /* latch + volume 15 for channels 0-3 */
    psg_port = 0xBF;
    psg_port = 0xDF;
    psg_port = 0xFF;
}

static _Bool ask_instructions(void) {
    for (;;) {
        const char *answer;
        transcribe("enter g for game or i for instructions");   /* Apple II wording */
        answer = text_input("");
        if (answer[0] == 'g') return 0;
        if (answer[0] == 'i') return 1;
    }
}

static void frame_irq(void) {
    text_keyboard_irq();
    PSGFrame();
}

void main(void) {
    unsigned char page;

    psg_silence();
    gfx_init();
    text_clear_history();
    SG_setFrameInterruptHandler(frame_irq);
    PSGPlayNoRepeat((void *)title_music);   /* title screen only, once */
    draw_title();
    text_wait_key();
    PSGStop();
    gfx_white_rows(0, TEXT_ROWS);           /* the title colours the trees */
    text_page(public_domain_text);

    show_image = 0;             /* the start prompt uses the full screen */
    gfx_clear_rows(0, TEXT_ROWS);
    text_clear_history();
    if (ask_instructions())
        for (page = 0; page < NUM_INSTRUCTION_PAGES; ++page)
            text_page(instruction_text[page]);

    show_image = 1;
    gfx_clear_rows(0, TEXT_ROWS);
    begin_game();
    for (;;) game_turn();
}

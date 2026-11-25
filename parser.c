#include "parser.h"
#include "rules.h"
#include "vocabulary.h"
#include "rooms.h"
#include "general.h"
#include "views.h"

#define WORD_LEN        8
#define NOT_DESCRIBED   0xFF    /* forces the room description */

static const char cmd_prompt[]   = "> ";
static const char que_prompt[]   = "? ";
static const char again_prompt[] = "would you like to play again? ";

unsigned char cur_verb;
unsigned char cur_noun;
unsigned char held;
_Bool command_handled;

static const char *prompt;
static unsigned char described_room;
static unsigned char described_temp;

/* "i dont know how to <verb> a <noun>": 19 + 8 + 3 + 8 characters */
static char msg_buf[TEXT_COLS];

static char *append(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    *dst = 0;
    return dst;
}

/* cut the next space separated field (Lua split(input, " ") semantics:
   consecutive spaces yield empty fields) to at most 8 characters */
static const char *next_field(const char *src, char *word) {
    unsigned char n = 0;
    while (*src && *src != ' ') {
        if (n < WORD_LEN) word[n++] = *src;
        ++src;
    }
    word[n] = 0;
    return (*src == ' ') ? src + 1 : 0;
}

static void start_game(void) {
    game_init();
    prompt = cmd_prompt;
    described_room = described_temp = NOT_DESCRIBED;
    command_handled = 1;
    text_clear_history();
}

static void run_timers(void) {
    if (prompt_replay) return;

    blank_image = 0;

    if (game.match_duration == 1) transcribe("the match went out");
    if (game.match_duration == 0) game.match_duration = 1;
    --game.match_duration;

    if (game.turn_count >= 36 && game.match_duration == 0) {
        if (!key_in_inventory(OBJ_LIT_CANDLE)) {
            blank_image = 1;
            transcribe("it is dark, you can't see");
        }
    }

    if (game.turn_count >= 20 && !game.darkness_arrived) transcribe("it is getting dark");
    game.darkness_arrived = (game.turn_count >= 35);

    /* win condition is off by one if we check "current room" */
    if (game.next_room == ROOM_FRONT_YARD) {
        if (key_in_inventory(OBJ_JEWELS) && game.daisy_dead) {
            transcribe("congratulations you have beaten adventure and are declared a guru wizard");
            replay_game();
        }
    }

    ++game.turn_count;
}

static void universal_fallback(const char *p_verb, const char *p_noun) {
    unsigned char room_obj;

    command_handled = 1;
    room_obj = noun_in_room(cur_noun);

    if (cur_verb == V_NONE) {
        append(append(append(msg_buf, "i dont know how to "), p_verb), " something");
        transcribe(msg_buf);
    } else if (cur_noun == N_NONE) {
        append(append(append(append(msg_buf, "i dont know how to "), p_verb), " a "), p_noun);
        transcribe(msg_buf);
    } else if (cur_verb == V_GET && room_obj != NO_OBJECT) {
        if (!obj_descr[room_obj])
            transcribe("it doesnt move");
        else
            add_key_to_inventory(room_obj);     /* also leaves room / temp room */
    } else if (cur_verb == V_GET) {
        transcribe("i dont see it here");
    } else if (cur_verb == V_DROP && held != NO_OBJECT) {
        set_temp_room(0);
        add_key_to_room(held, game.current_room);
    } else if (cur_verb == V_LOOK) {
        transcribe("there is nothing special");
    } else if (cur_verb == V_GO && (cur_noun == N_N || cur_noun == N_S || cur_noun == N_E ||
                                    cur_noun == N_W || cur_noun == N_U || cur_noun == N_D)) {
        transcribe("i cant go in that direction");
    } else {
        transcribe("i dont understand what you mean");
    }
}

static void parse_input(const char *input) {
    char p_verb[WORD_LEN + 1], p_noun[WORD_LEN + 1];
    const char *rest;

    command_handled = 0;

    if (prompt_replay) {
        if (input[0] == 'n') {
            quit_game();
        } else {
            start_game();
        }
        return;
    }

    /* <RETURN> means "toggle image" */
    if (!input[0]) {
        show_image = !show_image;
        view_dirty = 1;
        command_handled = 1;
        prompt = que_prompt;
        return;
    }
    prompt = cmd_prompt;

    rest = next_field(input, p_verb);
    p_noun[0] = 0;
    if (rest) next_field(rest, p_noun);

    cur_verb = parse_verb(p_verb);
    if (is_direction(cur_verb) && !p_noun[0]) {
        append(p_noun, p_verb);
        cur_noun = direction_noun(cur_verb);
        cur_verb = V_GO;
    } else {
        cur_noun = parse_noun(p_noun);
    }

    /* rooms can intercept actions before/after parser; see dining room fire */
    room_before();

    if (!command_handled) {
        held = NO_OBJECT;
        room_dispatch();
        if (command_handled && cur_verb == V_GO) set_temp_room(0);
    }

    held = noun_in_inventory(cur_noun);
    if (!command_handled)
        general_dispatch();

    if (!command_handled)
        universal_fallback(p_verb, p_noun);

    room_after();
}

/* the part of _update60() that runs after a handled command */
static void show_room(void) {
    game.current_room = game.next_room;
    view_update();
    if (prompt == cmd_prompt && !prompt_replay &&
        (described_room != game.current_room || described_temp != game.temp_room)) {
        transcribe(room_descr[game.current_room]);
        described_room = game.current_room;
        described_temp = game.temp_room;
    }
}

void begin_game(void) {
    start_game();
    run_timers();
}

void game_turn(void) {
    const char *input;

    if (command_handled) show_room();

    input = text_input(prompt_replay ? again_prompt : prompt);
    parse_input(input);
    if (command_handled) run_timers();
}

#ifndef PARSER_H
#define PARSER_H

/* parse_input() and run_timers() of mh.p8 */

/* start_game() followed by the timer tick that runs on game start */
void begin_game(void);

/* One command: show the room, read a line, dispatch it, run the timers. */
void game_turn(void);

#endif

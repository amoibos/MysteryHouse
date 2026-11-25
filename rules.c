#include "rules.h"

_Bool on(unsigned char v, unsigned char n) {
    if (cur_verb != v || cur_noun != n) return 0;
    return command_handled = 1;
}

_Bool run_rules(const sRule *rule) {
    for (; rule->verb; rule++) {
        if (on(rule->verb, rule->noun)) {
            if (rule->msg) transcribe(rule->msg);
            else game.next_room = rule->room;
            return 1;
        }
    }
    return 0;
}

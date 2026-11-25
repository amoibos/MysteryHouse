#include "vocabulary.h"

static signed char str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return (signed char)((unsigned char)*a - (unsigned char)*b);
}

/* the word tables are sorted by tools/mh_convert.py: binary search */
static unsigned char lookup(const char *word, const char * const *words,
                            const unsigned char *ids, unsigned char count) {
    unsigned char lo = 0, hi = count;
    while (lo < hi) {
        unsigned char mid = (lo + hi) >> 1;
        signed char c = str_cmp(word, words[mid]);
        if (c == 0) return ids[mid];
        if (c < 0) hi = mid;
        else       lo = mid + 1;
    }
    return 0;
}

unsigned char parse_verb(const char *word) {
    return lookup(word, verb_words, verb_ids, NUM_VERB_WORDS);
}

unsigned char parse_noun(const char *word) {
    return lookup(word, noun_words, noun_ids, NUM_NOUN_WORDS);
}

_Bool is_direction(unsigned char verb) {
    return direction_noun(verb) != N_NONE;
}

unsigned char direction_noun(unsigned char verb) {
    switch (verb) {
    case V_N: return N_N;
    case V_S: return N_S;
    case V_E: return N_E;
    case V_W: return N_W;
    case V_U: return N_U;
    case V_D: return N_D;
    default:  return N_NONE;
    }
}

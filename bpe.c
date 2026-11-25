#include "bpe.h"

#define FIRST_CODE      0x80

static char expanded[BPE_MAX_TEXT];

void bpe_open(sBpeReader *r, const char *text) {
    r->src = (const unsigned char *)text;
    if (*r->src == BPE_MARK) ++r->src;   /* plain text passes through */
    r->sp = 0;
}

char bpe_getc(sBpeReader *r) {
    for (;;) {
        unsigned char c;
        if (r->sp) {
            c = r->stack[--r->sp];
        } else {
            c = *r->src;
            if (!c) return 0;
            ++r->src;
        }
        if (c < FIRST_CODE) return (char)c;
        r->stack[r->sp++] = bpe_pairs[c - FIRST_CODE][1];
        r->stack[r->sp++] = bpe_pairs[c - FIRST_CODE][0];
    }
}

const char *bpe_text(const char *msg) {
    sBpeReader r;
    unsigned char len = 0;
    char c;

    if (!msg || *msg != BPE_MARK) return msg;
    bpe_open(&r, msg);
    while ((c = bpe_getc(&r)) && len < BPE_MAX_TEXT - 1)
        expanded[len++] = c;
    expanded[len] = 0;
    return expanded;
}

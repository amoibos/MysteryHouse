#ifndef BPE_H
#define BPE_H

/* Byte pair encoded messages, packed at build time by tools/pack_texts.py.
   A packed string starts with BPE_MARK; codes >= 0x80 stand for a pair of
   codes in bpe_pairs[]. */

#define BPE_MARK        0x01
#define BPE_MAX_TEXT    160     /* longest expanded message + 1 */
#define BPE_STACK       14      /* pair nesting depth (MAX_DEPTH in pack_texts.py) + 2 */

extern const unsigned char bpe_pairs[][2];

/* expanded copy of msg (in a static buffer) if it is packed, else msg */
const char *bpe_text(const char *msg);

/* character by character, for texts longer than the buffer (intro pages);
   a reader may be copied to look ahead */
typedef struct {
    const unsigned char *src;
    unsigned char sp;
    unsigned char stack[BPE_STACK];
} sBpeReader;

void bpe_open(sBpeReader *r, const char *text);
char bpe_getc(sBpeReader *r);           /* 0 at the end */

#endif

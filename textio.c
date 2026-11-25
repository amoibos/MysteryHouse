#include "textio.h"
#include "state.h"
#include "gfx.h"
#include "SGlib/SGlib.h"
#include "bpe.h"
#include "assets/font6.h"

#define HISTORY_LINES   23      /* the full screen text view */
#define LINE_LEN        TEXT_COLS
#define INPUT_ROW       (TEXT_ROWS - 1)
#define CURSOR_CHAR     '_'
#define BLINK_FRAMES    16

#define KEY_RETURN      '\r'
#define KEY_DELETE      '\b'

/* -- transcript history (ring buffer) -------------------------------------- */

/* Like the Apple II, whose text page keeps the lines that scrolled out of the
   4-line window, the history lives in video memory: the sprite pattern
   table at 0x3800 is unused (no sprites), 2 KB.  One line = 40 bytes,
   zero padded. */
#define HISTORY_VRAM    0x3800
#define history_addr(i) (HISTORY_VRAM + (unsigned int)(i) * LINE_LEN)

static unsigned char hist_head;     /* next line to write */
static unsigned char hist_count;
static unsigned char unseen;        /* lines added since the player last looked */

static char input_buf[LINE_LEN + 1];

/* -- VRAM text output ------------------------------------------------------ */

/* 6 pixel wide characters: 40 columns centred in the 256 pixel screen
   (one blank tile left and right).  Four characters are 24 pixels = three
   tiles, so each group of four is merged with fixed shifts:
       tile 0 = c0 | c1 >> 6,  tile 1 = c1 << 2 | c2 >> 4,  tile 2 = c2 << 4 | c3 >> 2
   The rest of the row after the text is cleared with one VRAM fill. */
static const unsigned char *glyph_of(char ch) {
    if (ch <= ' ' || ch > '~') return font6;    /* blank */
    return &font6[(unsigned char)(ch - ' ') << 3];
}

static void put_row(unsigned char y, const char *str) {
    unsigned char group[24];
    unsigned char len = 0, g, r;
    unsigned int addr = PGTADDRESS + ((unsigned int)y << 8) + 8;

    while (len < TEXT_COLS && str[len]) ++len;
    SG_VRAMmemset(addr - 8, 0, 8);
    for (g = 0; g < len; g += 4, addr += 24) {
        const unsigned char *c0 = glyph_of(str[g]);
        const unsigned char *c1 = glyph_of(g + 1 < len ? str[g + 1] : ' ');
        const unsigned char *c2 = glyph_of(g + 2 < len ? str[g + 2] : ' ');
        const unsigned char *c3 = glyph_of(g + 3 < len ? str[g + 3] : ' ');
        for (r = 0; r < 8; ++r) {
            group[r]      = c0[r] | (c1[r] >> 6);
            group[r + 8]  = (unsigned char)(c1[r] << 2) | (c2[r] >> 4);
            group[r + 16] = (unsigned char)(c2[r] << 4) | (c3[r] >> 2);
        }
        SG_VRAMmemcpy(addr, group, 24);
    }
    SG_VRAMmemset(addr, 0, PGTADDRESS + ((unsigned int)(y + 1) << 8) - addr);
}

static unsigned char visible_rows(void) {
    return show_image ? INPUT_ROW - TEXT_ROW_FIRST : INPUT_ROW;
}

static void read_history(unsigned char idx, char *line) {
    unsigned int addr = history_addr(idx);
    unsigned char i;
    for (i = 0; i < LINE_LEN; ++i)
        if (!(line[i] = SG_readVRAMbyte(addr + i))) break;
    line[i] = 0;
}

void text_refresh(void) {
    char line[LINE_LEN + 1];
    unsigned char rows = visible_rows();
    unsigned char first = INPUT_ROW - rows;
    unsigned char i;
    for (i = 0; i < rows; ++i) {
        signed char n = (signed char)hist_count - rows + i;     /* line age */
        if (n < 0) {
            put_row(first + i, "");
        } else {
            read_history((hist_head + HISTORY_LINES - hist_count + n) % HISTORY_LINES, line);
            put_row(first + i, line);
        }
    }
}

void text_clear_history(void) {
    hist_head = hist_count = unseen = 0;
    text_refresh();
}

/* -- keyboard -------------------------------------------------------------- */

/* SC-3000 matrix as returned by SG_getKeycodes(): row << 12 | bit, bits 0-7
   are PPI port A, bits 8-11 port B.  0 = key not used. */
static const char key_map[8][9] = {
    /* PA0  PA1  PA2  PA3  PA4         PA5         PA6         PA7  PB0 */
    { '1', 'q', 'a', 'z', 0,          ',',        'k',        'i', '8' },
    { '2', 'w', 's', 'x', ' ',        '.',        'l',        'o', '9' },
    { '3', 'e', 'd', 'c', 0,          '/',        ';',        'p', '0' },
    { '4', 'r', 'f', 'v', KEY_DELETE, 0,          ':',        '@', '-' },
    { '5', 't', 'g', 'b', 0,          0,          ']',        '[', '^' },
    { '6', 'y', 'h', 'n', 0,          KEY_DELETE, KEY_RETURN, 0,   0   },
    { '7', 'u', 'j', 'm', 0,          0,          0,          0,   0   },
    { 0,   0,   0,   0,   0,          0,          0,          0,   0   },
};

#define KC_SHIFT        0x6800      /* row 6, port B bit 3 */
#define MAX_SCAN_KEYS   4

static unsigned int prev_keys[MAX_SCAN_KEYS];
static unsigned char prev_count;

static char keycode_to_char(unsigned int code, _Bool shift) {
    unsigned char row = code >> 12;
    unsigned int bits = code & 0x0FFF;
    unsigned char bit = 0;
    char ch;
    while (bits > 1) {
        bits >>= 1;
        ++bit;
    }
    if (bit > 8) return 0;
    ch = key_map[row][bit];
    if (shift) {
        if (ch == '7') return '\'';
        if (ch == '/') return '?';
        if (ch == '1') return '!';
    }
    return ch;
}

/* the first key that is down now but was not down in the previous frame */
static char scan_key(void) {
    unsigned int keys[MAX_SCAN_KEYS];
    unsigned char n = SG_getKeycodes(keys, MAX_SCAN_KEYS);
    unsigned char i, j;
    _Bool shift = 0;
    char result = 0;

    for (i = 0; i < n; ++i)
        if (keys[i] == KC_SHIFT) shift = 1;

    for (i = 0; i < n && !result; ++i) {
        _Bool was_down = 0;
        if (keys[i] == KC_SHIFT) continue;
        for (j = 0; j < prev_count; ++j)
            if (prev_keys[j] == keys[i]) was_down = 1;
        if (!was_down) result = keycode_to_char(keys[i], shift);
    }

    for (i = 0; i < n; ++i) prev_keys[i] = keys[i];
    prev_count = n;
    return result;
}

/* The keyboard is scanned in the frame interrupt, so keys typed while a
   picture is being drawn are queued instead of lost. */
#define KEY_QUEUE       16          /* power of two */

static volatile char key_queue[KEY_QUEUE];
static volatile unsigned char key_head, key_tail;

void text_keyboard_irq(void) {
    char ch = scan_key();
    if (ch && ((key_head + 1) & (KEY_QUEUE - 1)) != key_tail) {
        key_queue[key_head] = ch;
        key_head = (key_head + 1) & (KEY_QUEUE - 1);
    }
}

static char get_key(void) {
    char ch;
    if (key_head == key_tail) return 0;
    ch = key_queue[key_tail];
    key_tail = (key_tail + 1) & (KEY_QUEUE - 1);
    return ch;
}

static char wait_char(void) {
    char ch;
    while (!(ch = get_key())) SG_waitForVBlank();
    return ch;
}

void text_wait_key(void) {
    wait_char();
}

/* -- transcript ------------------------------------------------------------ */

/* keys typed at "-more-" also start the next input line */
static char typed_ahead[LINE_LEN];
static unsigned char ahead_len;

static void more(void) {
    char ch;
    put_row(INPUT_ROW, "-more-");
    ch = wait_char();
    if (ch != KEY_RETURN && ch != KEY_DELETE && ahead_len < LINE_LEN)
        typed_ahead[ahead_len++] = ch;
    put_row(INPUT_ROW, "");
    unseen = 0;
}

static void push_line(const char *line, unsigned char len) {
    unsigned int addr = history_addr(hist_head);
    if (unseen >= visible_rows()) more();
    if (len) SG_VRAMmemcpy(addr, (void *)line, len);
    if (len < LINE_LEN) SG_VRAMmemset(addr + len, 0, LINE_LEN - len);
    hist_head = (hist_head + 1) % HISTORY_LINES;
    if (hist_count < HISTORY_LINES) ++hist_count;
    ++unseen;
    text_refresh();
}

/* compose_lines(): greedy word wrap at the screen width, "\n" breaks */
void transcribe(const char *msg) {
    char line[LINE_LEN + 1];
    unsigned char len = 0;

    msg = bpe_text(msg);                /* messages are packed at build time */
    if (!msg || !*msg) return;
    for (;;) {
        const char *word = msg;
        unsigned char wlen = 0;
        while (word[wlen] && word[wlen] != ' ' && word[wlen] != '\n') ++wlen;

        if (len && len + 1 + wlen > LINE_LEN) {
            push_line(line, len);
            len = 0;
        } else if (len) {
            line[len++] = ' ';
        }
        while (wlen && len < LINE_LEN) {
            line[len++] = *msg++;
            --wlen;
        }
        msg += wlen;                    /* cut over-long words */

        if (*msg == ' ') {
            ++msg;
        } else {
            push_line(line, len);
            len = 0;
            if (!*msg) break;
            ++msg;                      /* '\n' */
        }
    }
}

/* -- input line ------------------------------------------------------------ */

const char *text_input(const char *prompt) {
    unsigned char plen = 0, len = 0, blink = 0;
    unsigned char max;
    char line[LINE_LEN + 1];

    while (prompt[plen]) line[plen] = prompt[plen], ++plen;
    max = LINE_LEN - 1 - plen;
    unseen = 0;
    while (len < ahead_len && len < max) {
        line[plen + len] = input_buf[len] = typed_ahead[len];
        ++len;
    }
    ahead_len = 0;

    for (;;) {
        char ch;
        if (blink == 0 || blink == BLINK_FRAMES) {
            line[plen + len] = (blink == 0) ? CURSOR_CHAR : ' ';
            line[plen + len + 1] = 0;
            put_row(INPUT_ROW, line);
        }

        SG_waitForVBlank();
        blink = (blink + 1) % (BLINK_FRAMES * 2);
        ch = get_key();
        if (!ch) continue;
        blink = 0;

        if (ch == KEY_RETURN) break;
        if (ch == KEY_DELETE) {
            if (len) --len;
        } else if (len < max) {
            line[plen + len] = ch;
            input_buf[len++] = ch;
        }
    }

    input_buf[len] = 0;
    line[plen + len] = 0;
    put_row(INPUT_ROW, "");
    push_line(line, plen + len);
    unseen = 0;
    return input_buf;
}

/* -- full screen pages ----------------------------------------------------- */

static _Bool word_char(char c) {
    return c && c != ' ' && c != '\n' && c != '\t';
}

/* the intro pages are packed and longer than the bpe_text() buffer, so they
   are expanded on the fly; a copy of the reader measures the next word */
void text_page(const char *text) {
    sBpeReader in, ahead;
    char row[TEXT_COLS + 1];
    unsigned char x = 0, y = 0;
    char c;

    gfx_clear_rows(0, TEXT_ROWS);
    bpe_open(&in, text);
    c = bpe_getc(&in);

    while (c) {
        unsigned char wlen = 0;
        char a = c;
        ahead = in;
        while (word_char(a)) {
            ++wlen;
            a = bpe_getc(&ahead);
        }
        if (x && x + wlen > TEXT_COLS) {
            row[x] = 0;
            put_row(y++, row);
            x = 0;
        }
        if (y >= INPUT_ROW) {
            put_row(INPUT_ROW, "-more-");
            wait_char();
            gfx_clear_rows(0, TEXT_ROWS);
            y = 0;
        }
        while (word_char(c)) {
            if (x < TEXT_COLS) row[x++] = c;
            c = bpe_getc(&in);
        }
        if (c == '\t') {                /* paragraph indent */
            if (x < TEXT_COLS) row[x++] = ' ';
            if (x < TEXT_COLS) row[x++] = ' ';
        } else if (c == ' ') {
            if (x < TEXT_COLS) row[x++] = ' ';
        } else if (c == '\n') {
            row[x] = 0;
            put_row(y++, row);
            x = 0;
        }
        if (c) c = bpe_getc(&in);
    }
    row[x] = 0;
    if (y < INPUT_ROW) put_row(y, row);

    wait_char();
}

void text_wait_and_reset(void) {
    wait_char();
    __asm
        rst 0
    __endasm;
}

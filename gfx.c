#include "gfx.h"
#include "SGlib/SGlib.h"
#include "assets/font6.h"

unsigned char gfx_clip_bottom = IMAGE_H;

static const unsigned char bit_mask[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

void gfx_init(void) {
    SG_displayOff();
    SG_initBMPmode(SG_COLOR_BLACK, SG_COLOR_WHITE);
    SG_setBackdropColor(SG_COLOR_BLACK);
    gfx_clear_rows(0, SCREEN_ROWS);
    SG_displayOn();
}

void gfx_clear_rows(unsigned char first_row, unsigned char rows) {
    SG_VRAMmemset(PGTADDRESS + ((unsigned int)first_row << 8), 0, (unsigned int)rows << 8);
}

void gfx_white_rows(unsigned char first_row, unsigned char rows) {
    SG_VRAMmemset(CGTADDRESS + ((unsigned int)first_row << 8),
                  (unsigned char)((SG_COLOR_WHITE << 4) | SG_COLOR_BLACK), (unsigned int)rows << 8);
}

#define PATTERN_ADDR(x, y)  (((unsigned int)((y) & 0xF8) << 5) | ((x) & 0xF8) | ((y) & 7))

static void set_bits(unsigned int addr, unsigned char mask) {
    SG_writeVRAMbyte(addr, SG_readVRAMbyte(addr) | mask);
}

static void plot(unsigned char x, unsigned char y) {
    if (y >= gfx_clip_bottom) return;
    set_bits(PATTERN_ADDR(x, y), bit_mask[x & 7]);
}

/* Bresenham, endpoints included */
void gfx_line(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
    int dx = (x1 > x0) ? x1 - x0 : x0 - x1;
    int dy = (y1 > y0) ? y1 - y0 : y0 - y1;
    signed char sx = (x1 > x0) ? 1 : -1;
    signed char sy = (y1 > y0) ? 1 : -1;
    int err = dx - dy;
    int e2;

    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* labels inside pictures: 6 pixel font ORed in at any pixel position */
void gfx_text(unsigned char x, unsigned char y, const char *str) {
    for (; *str && x < SCREEN_W - 5; ++str, x += 6) {
        const unsigned char *glyph;
        unsigned char r, shift = x & 7;
        if (*str <= ' ' || *str > '~') continue;
        glyph = &font6[(unsigned char)(*str - ' ') << 3];
        for (r = 0; r < 7; ++r) {
            unsigned char row = y + r;
            unsigned int addr;
            if (row >= gfx_clip_bottom || !glyph[r]) continue;
            addr = PATTERN_ADDR(x, row);
            set_bits(addr, glyph[r] >> shift);
            if (shift > 2) set_bits(addr + 8, glyph[r] << (8 - shift));
        }
    }
}

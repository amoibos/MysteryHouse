#include "pictures.h"
#include "gfx.h"
#include "gamedata.h"
#include "SGlib/SGlib.h"

/* picture stream, written by tools/apple2.py (encode()) */
#define OP_LONG     0xF0        /* dx dy: signed line */
#define OP_ABS      0xF1        /* x y: line to local point */
#define OP_HORZ     0xF2        /* dx: horizontal line */
#define OP_VERT     0xF3        /* dy: vertical line */
#define OP_TEXT     0xFD        /* x y text 0: label */
#define OP_MOVE     0xFE        /* x y: start a polyline */
#define OP_END      0xFF

/* local coordinates + origin; past the screen edge is clamped (Pico-8
   wrapped x there, which only ever hit a dropped gun) */
static unsigned char clamp(unsigned int origin, unsigned char v) {
    origin += v;
    return origin > 255 ? 255 : (unsigned char)origin;
}

static void draw_stream(const unsigned char *p, unsigned int ox, unsigned int oy) {
    unsigned char x = 0, y = 0;         /* current point, local coordinates */
    unsigned char nx, ny;

    for (;;) {
        unsigned char op = *p++;
        if (op == OP_END) break;
        if (op == OP_MOVE) {
            x = p[0];
            y = p[1];
            p += 2;
            continue;
        }
        if (op == OP_TEXT) {
            gfx_text(clamp(ox, p[0]), clamp(oy, p[1]), (const char *)p + 2);
            p += 2;
            while (*p++) ;
            continue;
        }
        if (op < OP_LONG) {
            nx = x + (op >> 4) - 7;
            ny = y + (op & 15) - 7;
        } else if (op == OP_LONG) {
            nx = x + (signed char)p[0];
            ny = y + (signed char)p[1];
            p += 2;
        } else if (op == OP_ABS) {
            nx = p[0];
            ny = p[1];
            p += 2;
        } else if (op == OP_HORZ) {
            nx = x + (signed char)*p++;
            ny = y;
        } else {                        /* OP_VERT */
            nx = x;
            ny = y + (signed char)*p++;
        }
        gfx_line(clamp(ox, x), clamp(oy, y), clamp(ox, nx), clamp(oy, ny));
        x = nx;
        y = ny;
    }
}

void draw_image(unsigned char pic, unsigned char ox, unsigned char oy) {
    const unsigned char *data;
    if (!pic || pic > NUM_PICTURES) return;
    if (ox >= 128 || oy >= 128) return;         /* moved out of sight ({200,200}) */
    data = picture_data[pic];
    draw_stream(data, (unsigned int)ox << 1, (unsigned int)oy << 1);
}

void draw_title(void) {
    SG_loadZX7compressedBGTiles(title_patterns, 0);
    SG_loadZX7compressedBGColors(title_colours, 0);
}

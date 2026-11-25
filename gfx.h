#ifndef GFX_H
#define GFX_H

/* Drawing into the Graphics II bitmap (SG_initBMPmode: name table maps
   every screen cell to its own pattern, so pattern address = (y/8)*256 +
   (x/8)*8 + y%8).  Pixels are only ever set. */

#define SCREEN_W        256
#define SCREEN_ROWS     24      /* 8 pixel rows */
#define IMAGE_H         160     /* picture area of the Apple II original */

/* pixels with y >= gfx_clip_bottom are not drawn */
extern unsigned char gfx_clip_bottom;

void gfx_init(void);
void gfx_clear_rows(unsigned char first_row, unsigned char rows);
void gfx_white_rows(unsigned char first_row, unsigned char rows);   /* colour table */
void gfx_line(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1);
void gfx_text(unsigned char x, unsigned char y, const char *str);

#endif

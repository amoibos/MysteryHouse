#ifndef PICTURES_H
#define PICTURES_H

/* draw_image() of mh.p8 with the Apple II pictures (see tools/apple2.py):
   pic is a PIC_* / SPR_* number, (ox, oy) the Pico-8 position of an object;
   the picture is drawn with its origin at (2 * ox, 2 * oy). */
void draw_image(unsigned char pic, unsigned char ox, unsigned char oy);

/* Apple II title screen: ZX7 packed pattern and colour tables, full screen */
void draw_title(void);

#endif

/* **************************************************
   SGlib - C programming library for the SEGA SG-1000
   ( part of devkitSMS - github.com/sverx/devkitSMS )
   code: na_th_an, sverx
   ************************************************** */

/* Mystery House: trimmed to the functions the game calls (no sprites,
   tilemaps, joypad, pixel or ZX7-to-RAM routines), see devkitSMS for
   the full library. */

#include "SGlib.h"
#include <stdbool.h>

/* define VDPControlPort (SDCC z80 syntax) */
__sfr __at 0xBF VDPControlPort;
/* define VDPStatusPort */
__sfr __at 0xBF VDPStatusPort;
/* define VDPDataPort */
__sfr __at 0xBE VDPDataPort;
/* define VDPVcounter */
__sfr __at 0x7E VDPVCounterPort;
/* define VDPHcounter */
__sfr __at 0x7F VDPHCounterPort;
/* define IOPort (joypad) */
__sfr __at 0xDC IOPortL;
/* define IOPort (joypad) */
__sfr __at 0xDD IOPortH;
/* define PPI_A */
__sfr __at 0xDC SC_PPI_A;
/* define PPI_B */
__sfr __at 0xDD SC_PPI_B;
/* define PPI_C */
__sfr __at 0xDE SC_PPI_C;
/* define SC_PPI_CTRL */
__sfr __at 0xDF SC_PPI_CTRL;

#define HI(x)       ((x)>>8)
#define LO(x)       ((x)&0xFF)

#ifndef MAXSPRITES
#define MAXSPRITES      32
#endif

#define DISABLE_INTERRUPTS    __asm di __endasm
#define ENABLE_INTERRUPTS     __asm ei __endasm

#define WAIT_VRAM             __asm nop \
                                    nop \
                                    nop __endasm

/*  --------------------------------------------------------- *
        SG-1000 VRAM memory map:

    $0000 +---------+
          |   PGT   |  ($1800 bytes, pattern generator table)
    $1800 +---------+
          |   PNT   |  ($0300 bytes, nametable)
    $1B00 +---------+
          |   SAT   |  ($0080 bytes, sprite attribute table)
    $1B80 +---------+
          |         |  ($0480 bytes free)
    $2000 +---------+
          |   CGT   |  ($1800 bytes, colour table)
    $3800 +---------+
          |   SGT   |  ($0800 bytes, sprite generator table)
          +---------+
 *  --------------------------------------------------------- */

#define PNTADDRESS      0x1800
#define SATADDRESS      0x1B00
#define SGTADDRESS      0x3800
/* PGTADDRESS and CGTADDRESS are defined in SGlib.h */

/* the VDP registers initialization value */
const unsigned char VDPReg_init[8]={
  0x02, // Mode2
  0xa0, // 16KB, screen off, VBlank IRQ, sprite 8x8, no zoom
  0x06, // PN bits 13-10 = 0 1 1 0     (address = $1800)
  0xff, // CT bits 13-7  = 1 x x x x x x x (address = $2000)
  0x03, // PG bits 13-11 = 0 x x     (address = $0000)
  0x36, // SA bits 13-7  = 0 1 1 0 1 1 0   (address = $1B00)
  0x07, // SG bits 13-11 = 1 1 1     (address = $3800)
  0x01  // text color (unused in Mode2) / backdrop (black)
};

/* the VDP registers #0 and #1 'shadow' RAM */
unsigned char VDPReg[2]={0x02, 0xa0};

volatile bool VDPBlank;   // used by INTerrupt
/* not used by Mystery House: volatile bool   PauseRequested;   // used by NMI */

#ifdef AUTODETECT_SPRITE_OVERFLOW
unsigned char spriteOverflowFlipflop=0;
unsigned char spriteOverflowCounter;
volatile bool VDPSpriteCollision=false;
volatile bool VDPSpriteOverflow=false;
#endif

/* not used by Mystery House (joypad): volatile unsigned int KeysStatus, PreviousKeysStatus; */


#ifndef NO_FRAME_INT_HOOK
/* If non-NULL, will be called by SG_isr after acknowledging */
/* the interrupt and reading controller status */
void (*SG_theFrameInterruptHandler)(void);
#endif

#ifndef NESTED_DI_EI_SUPPORT
/* macro definitions (no nested DI/EI support) */
#define SG_write_to_VDPRegister(VDPReg,value)  do{DISABLE_INTERRUPTS; VDPControlPort=(value);     VDPControlPort=(VDPReg)|0x80;     ENABLE_INTERRUPTS;}while(0)
#define SG_set_address_VRAM(address)           do{DISABLE_INTERRUPTS; VDPControlPort=LO(address); VDPControlPort=HI(address)|0x40;  ENABLE_INTERRUPTS;}while(0)
#define SG_set_address_VRAM_read(address)      do{DISABLE_INTERRUPTS; VDPControlPort=LO(address); VDPControlPort=HI(address)&~0x40; ENABLE_INTERRUPTS;}while(0)
#else
/* inline __critical functions (nested DI/EI supported!) */
inline void SG_write_to_VDPRegister (unsigned char VDPReg, unsigned char value) {
  /* INTERNAL FUNCTION */
  __critical {
    VDPControlPort=value;
    VDPControlPort=VDPReg|0x80;
  }
}

inline void SG_set_address_CRAM (unsigned char address) {
  /* INTERNAL FUNCTION */
  __critical {
    VDPControlPort=address;
    VDPControlPort=0xC0;
  }
}

inline void SG_set_address_VRAM (unsigned int address) {
  /* INTERNAL FUNCTION */
  __critical {
    VDPControlPort=LO(address);
    VDPControlPort=HI(address)|0x40;
  }
}
#endif


inline void SG_byte_to_VDP_data (unsigned char data) {
  /* INTERNAL FUNCTION */
  VDPDataPort = data;
}

inline unsigned char  SG_byte_from_VDP_data (void) {
  /* INTERNAL FUNCTION */
  return(VDPDataPort);
}

inline void SG_byte_array_to_VDP_data (const unsigned char *data, unsigned int size) {
  /* INTERNAL FUNCTION */
  do {
    VDPDataPort = *(data ++);
  } while (-- size);
}

inline void SG_byte_brief_array_to_VDP_data (const unsigned char *data, unsigned char size) {
  /* INTERNAL FUNCTION */
  do {
    VDPDataPort = *(data ++);
  } while (-- size);
}

inline void SG_word_to_VDP_data (unsigned int data) {
  /* INTERNAL FUNCTION */
  VDPDataPort = LO (data);
  WAIT_VRAM;        /* ensure we're not pushing data too fast */
  VDPDataPort = HI (data);
}

void SG_VDPturnOnFeature (unsigned int feature) {
  /* turns on a VDP feature */
  VDPReg[HI(feature)]|=LO(feature);
  SG_write_to_VDPRegister(HI(feature), VDPReg[HI(feature)]);
}

void SG_VDPturnOffFeature (unsigned int feature) {
  /* turns off a VDP feature */
  VDPReg[HI(feature)]&=~LO(feature);
  SG_write_to_VDPRegister(HI(feature), VDPReg[HI(feature)]);
}

void SG_init (void) {
  unsigned char i;
  for (i=0;i<8;i++)
    SG_write_to_VDPRegister (i, VDPReg_init[i]);
  SG_writeVRAMbyte (SATADDRESS, 0xD0);   /* no sprites */
}

void SG_setBackdropColor (unsigned char entry) {
  SG_write_to_VDPRegister (0x07, entry);
}

void SG_initBMPmode (unsigned char bg_color, unsigned char fg_color) {
  unsigned char color_data = ((fg_color & 0x0F)  << 4) + (bg_color & 0x0F);
  unsigned int address;
  unsigned char tileno=0;

  //setup nametable
  address = PNTADDRESS;
  for (unsigned char y=0; y != SG_MAX_TILE_RES_Y; ++y) {
    for (unsigned char x=0; x != SG_MAX_TILE_RES_X; ++x) {
        SG_set_address_VRAM (address + x);
        SG_byte_to_VDP_data(tileno++);
    }
    address += 1 << 5;
  }

  //setup foreground and background for whole screen
  address = CGTADDRESS;
  for (unsigned char y=0; y != SG_MAX_TILE_RES_Y; ++y) {
    SG_set_address_VRAM (address);
    for (unsigned char x=SG_MAX_TILE_RES_X; x != 0; --x) {
      for(unsigned char t=8; t != 0; --t)
        VDPDataPort = color_data;
    }
    address += SG_MAX_TILE_RES_X << 3;
  }
}

void SG_waitForVBlank (void) {
  VDPBlank=false;
  while (!VDPBlank);
}

#define PPI_TEST_VALUE_1   0x55
#define PPI_TEST_VALUE_2   0xAA

unsigned char SG_getKeycodes (unsigned int *keys, unsigned char max_keys) {
    unsigned char count=0;

    for(unsigned char keyb_row=0; keyb_row < 8; keyb_row++) {
        unsigned int keyb_stat, row_no;

        SC_PPI_C = keyb_row;
        row_no = keyb_row << 12;
        keyb_stat=(~((SC_PPI_B << 8) | SC_PPI_A)) & 0x0FFF;
        for(unsigned int bit_mask=0x800; keyb_stat; bit_mask >>= 1) {
            if (keyb_stat & bit_mask) {
                if (count < max_keys)
                        keys[count++] = row_no + bit_mask;
                else
                    return count;
                keyb_stat -= bit_mask;
            }
        }
    }
    return count;
}

#pragma save
#pragma disable_warning 85
void SG_decompressZX7toVRAM (const void *src, unsigned int dst) __naked {
/* =====================================================================
* by Einar Saukas, Antonio Villena & Metalbrain
* modified for sms vram by aralbrec
* modified for asm by Maxim
* C wrapper/made interrupt safe/VDP timing safe by sverx
===================================================================== */
  __asm
  ld c,#0xbf     ; VDP control port
  set 6,d        ; set VRAM write bit
  di             ; set VRAM address
  out (c),e
  out (c),d
  ei
  res 6,d        ; remove VRAM write bit
  dec c          ; data port

  ld a,#0x80     ; Signal bit for flags byte (1<<7)

dzx7s_copy_byte_loop:
  outi           ; copy literal byte
  inc de

dzx7s_main_loop:

  call dzx7s_next_bit
  jr nc,dzx7s_copy_byte_loop     ; next bit indicates either literal or sequence

  ; determine number of bits used for length (Elias gamma coding)
  push de
  ld bc,#0
  ld d,b

dzx7s_len_size_loop:
  inc d
  call dzx7s_next_bit
  jr nc,dzx7s_len_size_loop

  ; determine length
dzx7s_len_value_loop:
  call nc,dzx7s_next_bit
  rl c
  rl b

  jp c,l_ret
  dec d
  jr nz,dzx7s_len_value_loop
  inc bc         ; adjust length

  ; determine offset
  ld e, (hl)     ; load offset flag (1 bit) + offset value (7 bits)
  inc hl
  sla e
  inc e
  jr nc,dzx7s_offset_end     ; if offset flag is set, load 4 extra bits
  ld d, #0x10                ; bit marker to load 4 bits

dzx7s_rld_next_bit:
  call dzx7s_next_bit
  rl d           ; insert next bit into D
  jr nc,dzx7s_rld_next_bit  ; repeat 4 times, until bit marker is out
  inc d          ; add 128 to DE
  srl d          ; retrieve fourth bit from D

dzx7s_offset_end:
  rr e           ; insert fourth bit into E

; copy previous sequence
  ex (sp),hl     ; store source, restore destination
  push hl        ; store destination
  sbc hl,de      ; HL = destination - offset - 1
  pop de         ; DE = destination
  push af

  ; ***********************
  set 6,d
  dec bc
  inc b
  inc c
  ld a,c
  ld c,#0xbf

dzx7s_outer_loop:
  push bc
  ld b,a

dzx7s_inner_loop:
  nop                     ; 4
  di                      ; 4 = 27 (safe on every Master System or Game Gear)
  out (c),l
  out (c),h
  ei             ; 4
  inc hl         ; 6
  xor a          ; 4
  ret nz         ; 5      (this ret will never happen, it is just to wait 5 cycles)
  nop            ; 4
  nop            ; 4 = 27 (safe on every Master System or Game Gear)
  in a,(#0xbe)

  nop            ; 4
  nop            ; 4
  nop            ; 4
  nop            ; 4
  nop            ; 4
  nop            ; 4
  di             ; 4 = 28 (safe on every Master System or Game Gear)
  out (c),e
  out (c),d
  ei
  out (#0xbe),a
  inc de                  ; 6
  djnz dzx7s_inner_loop   ; 13
  ld a,b
  pop bc
  djnz dzx7s_outer_loop
  ld c,b
  res 6,d
  ; ***********************

  pop af
  ld c,#0xbe
  pop hl         ; restore source address (compressed data)
  jr nc, dzx7s_main_loop

dzx7s_next_bit:
  add a,a        ; check next bit
  ret nz         ; no more bits left?
  ld a,(hl)      ; load another group of 8 bits
  inc hl
  rla
  ret

l_ret:
  pop hl
  ret
  __endasm;
}
#pragma restore

/* low level functions, just to be used for dirty tricks ;) */
unsigned char SG_readVRAMbyte (unsigned int address) {
  SG_set_address_VRAM_read (address);
  WAIT_VRAM;
  return VDPDataPort;
}

void SG_writeVRAMbyte (unsigned int address, unsigned char data) {
  SG_set_address_VRAM (address);
  VDPDataPort = data;
}

void SG_VRAMmemcpy (unsigned int dst, void *src, unsigned int size) {
  SG_set_address_VRAM(dst);
  SG_byte_array_to_VDP_data(src,size);
}

void SG_VRAMmemset (unsigned int dst, unsigned char value, unsigned int size) {
  SG_set_address_VRAM(dst);
  while (size>0) {
    SG_byte_to_VDP_data(value);
    size--;
  }
}

#ifndef NO_FRAME_INT_HOOK
void SG_setFrameInterruptHandler (void (*theHandlerFunction)(void)) __z88dk_fastcall {
  SG_theFrameInterruptHandler=theHandlerFunction;
}
#endif

/* Interrupt Service Routines */
void SG_isr (void) __critical __interrupt(0) {
  volatile unsigned char VDPStatus=VDPStatusPort;  /* this also aknowledge interrupt at VDP */
#ifdef AUTODETECT_SPRITE_OVERFLOW
  VDPSpriteOverflow=(VDPStatus & 0x40);
  VDPSpriteCollision=(VDPStatus & 0x20);
#endif
  if (VDPStatus & 0x80) {
    VDPBlank=true;         /* frame interrupt */
    /* read key input */
    /* not used by Mystery House (joypad):
    PreviousKeysStatus=KeysStatus;
    KeysStatus=~(((IOPortH)<<8)|IOPortL);
    */
#ifndef NO_FRAME_INT_HOOK
    if (SG_theFrameInterruptHandler) {
      SG_theFrameInterruptHandler();
    }
#endif
  }
  /* Z80 disable the interrupts on ISR, so we should re-enable them explicitly */
  ENABLE_INTERRUPTS;
}

void SG_nmi_isr (void) __critical __interrupt {   /* this is for NMI */
  /* not used by Mystery House: PauseRequested = true; */
}

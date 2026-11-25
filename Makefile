TARGET_PLATFORM := SC
OUTPUT := mystery_house

# mystery_house.SC: 48 KB ROM without mapper for any SC-3000 cartridge;
# code, data and assets are linked flat into 0x0000-0xBFFF.
# Objects go to build/.  The asset data is generated from original/ and
# apple2/ by tools/mh_convert.py.

COMPILER := sdcc
LINKER := ihx2sms
PYTHON := python
FLAGS := -mz80 -DPLATFORM_SG

OBJ := build
SRC := $(OBJ)/src

# messages are byte pair encoded at build time (tools/pack_texts.py): these
# modules are compiled from packed copies in build/src
PACKED_MODULES := rooms general objects gamedata texts
MODULES := main textio gfx pictures views parser rules state vocabulary bpe

RELS := $(addprefix $(OBJ)/,$(addsuffix .rel,$(MODULES)))
PACKED_RELS := $(addprefix $(OBJ)/,$(addsuffix .rel,$(PACKED_MODULES) bpe_table))
LIB_RELS := $(OBJ)/crt0_sg.rel $(OBJ)/SGlib.rel $(OBJ)/PSGlib.rel
ASSET_RELS := $(OBJ)/font6.rel $(OBJ)/assets.rel $(OBJ)/music.rel
PACK_INPUTS := $(addsuffix .c,rooms general objects gamedata) assets/texts.c
HEADERS := $(wildcard *.h)

all: $(OUTPUT).$(TARGET_PLATFORM)

# regenerate gamedata.c/.h, assets/assets.c and assets/texts.c from original/ and apple2/
data:
	$(PYTHON) tools/mh_convert.py

$(OBJ):
	$(PYTHON) -c "import os; os.makedirs('$(OBJ)', exist_ok=True)"

$(OBJ)/%.rel: %.c $(HEADERS) | $(OBJ)
	$(COMPILER) $(FLAGS) -c -o $@ $<

$(SRC)/bpe_table.c: $(PACK_INPUTS) tools/pack_texts.py | $(OBJ)
	$(PYTHON) tools/pack_texts.py --out $(SRC) --arrays room_descr,obj_descr $(PACK_INPUTS)

$(SRC)/%.c: $(SRC)/bpe_table.c
	@:

$(PACKED_RELS): $(OBJ)/%.rel: $(SRC)/%.c $(HEADERS)
	$(COMPILER) $(FLAGS) -I. -c -o $@ $<

# always assemble crt0 from source: an old prebuilt crt0_sg.rel set the stack
# to 0xC3F0 (1 KB SG-1000 RAM), right inside this game's variables
$(OBJ)/crt0_sg.rel: crt0/crt0_sg.s | $(OBJ)
	sdasz80 -g -o $@ $<

# SGlib is compiled with this SDCC too, so the calling convention matches
$(OBJ)/SGlib.rel: SGlib/SGlib.c SGlib/SGlib.h | $(OBJ)
	$(COMPILER) -mz80 -c -o $@ $<

# PSGlib from devkitSMS, music only (psg/PSGlib.c: "#" immediates patched
# for the inline assembler only, SDCC 4.6 rejects them in C)
$(OBJ)/PSGlib.rel: psg/PSGlib.c psg/PSGlib.h | $(OBJ)
	$(COMPILER) -mz80 --max-allocs-per-node 100000 -D PSGLIB_NOSFXCODE -c -o $@ $<

$(OBJ)/font6.rel: assets/font6.c | $(OBJ)
	$(COMPILER) $(FLAGS) -c -o $@ $<

$(OBJ)/assets.rel: assets/assets.c | $(OBJ)
	$(COMPILER) $(FLAGS) -c -o $@ $<

# title music, made by tools/make_music.py
$(OBJ)/music.rel: assets/music.c | $(OBJ)
	$(COMPILER) $(FLAGS) -c -o $@ $<

# crt0 first
$(OUTPUT).ihx: $(LIB_RELS) $(RELS) $(PACKED_RELS) $(ASSET_RELS)
	$(COMPILER) -mz80 --no-std-crt0 --data-loc 0xC000 -o $@ \
		$(LIB_RELS) $(RELS) $(PACKED_RELS) $(ASSET_RELS)

$(OUTPUT).$(TARGET_PLATFORM): $(OUTPUT).ihx
	$(LINKER) $(OUTPUT).ihx $(OUTPUT).$(TARGET_PLATFORM)

# game logic on the PC: walkthrough with plain and BPE packed messages
test:
	sh tests/run_tests.sh

clean:
	$(PYTHON) -c "import shutil, glob, os; shutil.rmtree('build', True); [os.remove(f) for e in ('ihx','map','noi','lk','cdb') for f in glob.glob('*.' + e)]"

.PHONY: all clean data test

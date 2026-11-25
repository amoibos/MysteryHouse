#!/bin/sh
# Compile the game logic for the host and play the walkthrough, once with
# the plain messages and once with the BPE packed ones (as in the ROM); both
# runs must win and print exactly the same text.
cd "$(dirname "$0")/.." || exit 1
PACKED=tests/packed
python tools/pack_texts.py --out $PACKED --arrays room_descr,obj_descr     rooms.c general.c objects.c gamedata.c assets/texts.c > /dev/null || exit 1
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -I."
COMMON="tests/host_io.c tests/asset_stub.c vocabulary.c state.c parser.c rules.c bpe.c $PACKED/bpe_table.c"
gcc $CFLAGS -o tests/mh_host.exe $COMMON gamedata.c objects.c general.c rooms.c || exit 1
gcc $CFLAGS -o tests/mh_host_packed.exe $COMMON     $PACKED/gamedata.c $PACKED/objects.c $PACKED/general.c $PACKED/rooms.c || exit 1
./tests/mh_host.exe < tests/walkthrough.txt > tests/walkthrough.out
./tests/mh_host_packed.exe < tests/walkthrough.txt > tests/walkthrough_packed.out
grep -q "congratulations you have beaten adventure" tests/walkthrough.out || { echo "FAIL: walkthrough not won"; exit 1; }
cmp -s tests/walkthrough.out tests/walkthrough_packed.out || { echo "FAIL: packed messages differ"; exit 1; }
echo "OK: walkthrough won, packed messages identical"

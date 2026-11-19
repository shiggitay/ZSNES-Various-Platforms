#!/bin/sh
# build-macos9.sh - build ZSNES as a classic Mac OS application for PowerPC
# Macs (Mac OS 8.6 to 9.2.2), with the Retro68 cross toolchain.
#
#   git clone https://github.com/autc04/Retro68 && cd Retro68 &&
#     git submodule update --init && mkdir ../build && cd ../build &&
#     ../Retro68/build-toolchain.bash --no-68k --no-carbon
#   RETRO68=/path/to/build/toolchain tools/build-macos9.sh
#
# Leaves build-macos9/ZSNES.bin (MacBinary), ZSNES.dsk (an HFS disk image
# holding the application) and ZSNES.pef (the bare code fragment). The
# frontend is the Unix one, built on macos9/toolbox.c - the SDL3 calls it
# makes, done with the Toolbox - and macos9/posix.c, which gives the File
# Manager Unix paths.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RETRO68=${RETRO68:-$(dirname "$(dirname "$(command -v powerpc-apple-macos-gcc)")")}
PATH=$RETRO68/bin:$PATH
CC=powerpc-apple-macos-gcc
OUT=${OUT:-$ROOT/build-macos9}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}

command -v $CC >/dev/null || { echo "need Retro68's $CC: set RETRO68" >&2; exit 1; }
cd "$ROOT"

# The core and the Unix frontend, as the Makefile lists them for an SDL 1.2
# build, with the parts that need Unix itself swapped for macos9/ ones.
SRCS=$(make -s --no-print-directory --eval 'zsnes-print-srcs: ; @echo $(SRCS)' zsnes-print-srcs \
    ARCH=LINUX CPU=ppc WITH_SDL12=yes WITH_SDL=yes WITH_PNG= WITH_OPENGL= BUILD_DIR="$OUT/unused" 2>/dev/null |
    tr ' ' '\n' | grep -v -e '^unix/sdl12compat.c$' -e '^unix/net_transport.c$' -e '^unix/safelib.c$' -e '^mmlib/linux.c$' | tr '\n' ' ')
SRCS="$SRCS unix/gl_draw.c macos9/toolbox.c macos9/gl.c macos9/posix.c macos9/net_stub.c macos9/safelib.c"

DEFS="-DZSNES_MACOS9 -D__UNIXSDL__ -DZSNES_SDL12 -D__OPENGL__ -DNO_PNG -DNO_DEBUGGER -D_GNU_SOURCE"
# Scheduled for the G3 and G4 (-mtune changes no instructions: it still
# runs on a 601 or a 604).
CFLAGS="-std=gnu11 -O2 -mtune=750 -fno-strict-aliasing -mminimal-toc \
 -Wall -Wno-unused-parameter -Wno-multichar -Wno-unused-but-set-variable -Wno-incompatible-pointer-types -Wno-format -Wno-char-subscripts -Werror=implicit-function-declaration \
 -I$ROOT/macos9/include -I$OUT -include $ROOT/macos9/compat.h ${MACOS9_CFLAGS:-}"

mkdir -p "$OUT"

# The configuration parsers: parsegen.py writes and compiles them.
for p in cfg input md; do
    if [ ! -f "$OUT/$p.o" ] || [ "$p.psr" -nt "$OUT/$p.o" ]; then
        echo "PSR $p"
        python3 ./parsegen.py $DEFS -gcc $CC -compile -flags "$CFLAGS $DEFS" \
            -cheader "$OUT/$p.h" -fname "$p" "$OUT/$p.o" "$p.psr"
    fi
done

# Everything else, in parallel through make.
{
    printf 'all:'
    for s in $SRCS; do printf ' %s' "$OUT/obj/${s%.c}.o"; done
    printf '\n'
    for s in $SRCS; do
        o=$OUT/obj/${s%.c}.o
        extra=
        [ "$s" = main.c ] && extra=-Dmain=zsnes_main
        printf '%s: %s %s/cfg.h %s/input.h %s/md.h\n' "$o" "$s" "$OUT" "$OUT" "$OUT"
        if grep -qE '"[^"]*\.(long|short)\b' "$s"; then
            # Files that lay out emulator state in inline asm. The XCOFF
            # assembler pads .long and .short to their natural alignment,
            # which moves every label after an odd-sized item; .4byte and
            # .2byte place the bytes exactly where the asm says, as ELF's
            # .long does. The compiler's own output pads explicitly, so the
            # rewrite leaves it unchanged.
            printf '\t@mkdir -p %s && echo "CC %s" && %s %s %s %s -S %s -o %s.s && sed -E %s %s.s > %s.fixed.s && %s -c %s.fixed.s -o %s\n' \
                "$(dirname "$o")" "$s" "$CC" "$CFLAGS" "$DEFS" "$extra" "$s" "$o" \
                "'s/^([[:space:]]*)\\.long([[:space:]])/\\1.4byte\\2/; s/^([[:space:]]*)\\.short([[:space:]])/\\1.2byte\\2/'" \
                "$o" "$o" "$CC" "$o" "$o"
        else
            printf '\t@mkdir -p %s && echo "CC %s" && %s %s %s %s -c %s -o %s\n' \
                "$(dirname "$o")" "$s" "$CC" "$CFLAGS" "$DEFS" "$extra" "$s" "$o"
        fi
    done
} > "$OUT/objs.mk"
make -s -k -j"$JOBS" -f "$OUT/objs.mk" all

OBJS=
for s in $SRCS; do OBJS="$OBJS $OUT/obj/${s%.c}.o"; done
# OpenGL and DrawSprocket are optional extensions on Mac OS 8 and 9, so both
# are imported weakly: the application launches without them and checks for
# them before use. OpenGL's import library is made here (Retro68 has none);
# DrawSprocket's is Retro68's, under a member name MakePEF reads as weak.
PATH=$RETRO68/bin:$PATH sh "$ROOT/macos9/implib/mkimplib.sh" OpenGLLibrary \
    "$ROOT/macos9/implib/OpenGLLibrary.txt" "$OUT/libOpenGLWeak.a"
DSP_TMP=$(mktemp -d)
(cd "$DSP_TMP" && powerpc-apple-macos-ar x "$RETRO68/powerpc-apple-macos/lib/libDrawSprocketLib.a" DrawSprocketLib.o &&
    mv DrawSprocketLib.o DrawSprocketLib__weak.o && rm -f "$OUT/libDrawSprocketWeak.a" &&
    powerpc-apple-macos-ar cq "$OUT/libDrawSprocketWeak.a" DrawSprocketLib__weak.o)
rm -rf "$DSP_TMP"

echo "LINK ZSNES.xcoff"
# shellcheck disable=SC2086
$CC -o "$OUT/ZSNES.xcoff" $OBJS "$OUT/cfg.o" "$OUT/input.o" "$OUT/md.o" \
    -lm -lAppearanceLib -L"$OUT" -lOpenGLWeak -lDrawSprocketWeak

echo "PEF ZSNES.pef"
MakePEF "$OUT/ZSNES.xcoff" -o "$OUT/ZSNES.pef"

echo "REZ ZSNES"
Rez "$ROOT/macos9/zsnes.r" -I"$RETRO68/powerpc-apple-macos/RIncludes" \
    -o "$OUT/ZSNES.bin" -t APPL -c ZSN2 --data "$OUT/ZSNES.pef"

# Rez cannot set Finder flags; without "has bundle" the Finder ignores the
# icons and will not let ROMs be dropped on the application. Set it in the
# MacBinary header (Finder flags, high byte, at 73; CRC at 124), then build
# the disk image from that with hfsutils, which keeps the flags.
python3 -I - "$OUT/ZSNES.bin" <<'PY'
import sys
p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
b[73] |= 0x20
c = 0
for x in b[:124]:
    c ^= x << 8
    for _ in range(8):
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
b[124], b[125] = c >> 8, c & 0xFF
open(p, 'wb').write(b)
PY
HFS_HOME=$(mktemp -d)
rm -f "$OUT/ZSNES.dsk"
dd if=/dev/zero of="$OUT/ZSNES.dsk" bs=1024 count=4096 2>/dev/null
HOME=$HFS_HOME hformat -l "ZSNES" "$OUT/ZSNES.dsk" >/dev/null
HOME=$HFS_HOME hcopy -m "$OUT/ZSNES.bin" ":ZSNES"
tr '\n' '\r' < "$ROOT/macos9/ReadMe.txt" > "$OUT/ReadMe.txt"
HOME=$HFS_HOME hcopy -r "$OUT/ReadMe.txt" ":ZSNES Read Me"
HOME=$HFS_HOME hattrib -t TEXT -c ttxt ":ZSNES Read Me"
HOME=$HFS_HOME hls -l
HOME=$HFS_HOME humount >/dev/null
rm -rf "$HFS_HOME"
ls -l "$OUT/ZSNES.bin" "$OUT/ZSNES.dsk"

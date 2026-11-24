#!/bin/sh
# build-ps3.sh - build ZSNES as PlayStation 3 homebrew with PSL1GHT, as an
# installable .pkg (and the EBOOT.BIN in it), for a PS3 running custom
# firmware or HEN.
#
#   git clone https://github.com/ps3dev/ps3toolchain && cd ps3toolchain &&
#     PS3DEV=/path/to/ps3dev ./toolchain.sh 1 2 4 5 6 8
#   PS3DEV=/path/to/ps3dev tools/build-ps3.sh
#
# Leaves build-ps3/zsnes-ps3.pkg, which installs ZSNES under Game in
# the XMB (title ID ZSNE00001), and build-ps3/pkg/USRDIR/EBOOT.BIN. The
# frontend is the Unix one built for SDL 1.2's set of calls, which ps3/ps3.c
# provides on the PS3's own libraries (RSX video, pads, audio).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PS3DEV=${PS3DEV:-/usr/local/ps3dev}
PSL1GHT=${PSL1GHT:-$PS3DEV}
export PS3DEV PSL1GHT
PATH=$PS3DEV/bin:$PS3DEV/ppu/bin:$PATH
OUT=${OUT:-$ROOT/build-ps3}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
CC=ppu-gcc
APPID=ZSNE00001
TITLE="ZSNES"
CONTENTID=UP0001-${APPID}_00-0000000000000000

command -v $CC >/dev/null || { echo "need PSL1GHT's $CC: set PS3DEV" >&2; exit 1; }
mkdir -p "$OUT"
cd "$ROOT"

# The core and the Unix frontend, as the Makefile lists them for an SDL 1.2
# build, with ps3/ps3.c for SDL and stand-ins for what the PS3 has no use for.
SRCS=$(make -s --no-print-directory --eval 'zsnes-print-srcs: ; @echo $(SRCS)' zsnes-print-srcs \
    ARCH=LINUX CPU=ppc WITH_SDL12=yes WITH_SDL=yes WITH_PNG= WITH_OPENGL= BUILD_DIR="$OUT/unused" 2>/dev/null |
    tr ' ' '\n' | grep -E '^[A-Za-z0-9_/.-]+\.c$' |
    grep -v -e '^unix/sdl12compat.c$' -e '^unix/sdl12gl.c$' -e '^unix/glupload.c$' -e '^unix/safelib.c$' \
        -e '^unix/net_transport.c$' -e '^mmlib/linux.c$' | tr '\n' ' ')
SRCS="$SRCS ps3/ps3.c ps3/posix.c macos9/safelib.c macos9/net_stub.c mmlib/linux.c"

DEFS="-DZSNES_PS3 -D__UNIXSDL__ -DZSNES_SDL12 -DNO_PNG -DNO_DEBUGGER -D_GNU_SOURCE"
# -mstrict-align: no load or store wider than the alignment the compiler can
# prove. The core reads SNES memory at any byte offset (unaligned.h), which
# compilers fold into single wide (on PowerPC, byte-reversed) loads; Linux
# fixes up the ones that straddle a boundary, but the PS3's system software
# does not, and the program dies. Byte loads instead, wherever unsure.
CFLAGS="-std=gnu11 -O2 -mcpu=cell -mhard-float -mstrict-align -ffunction-sections -fdata-sections \
 -fno-strict-aliasing -Wall -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
 -Wno-unused-but-set-variable -Wno-char-subscripts -Werror=implicit-function-declaration \
 -I$ROOT/ps3/include -I$OUT -I$PSL1GHT/ppu/include -I$PSL1GHT/ppu/include/simdmath \
 -include $ROOT/ps3/compat.h ${PS3_CFLAGS:-}"

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
        printf '%s: %s %s/cfg.h %s/input.h %s/md.h\n' "$o" "$s" "$OUT" "$OUT" "$OUT"
        printf '\t@mkdir -p %s && echo "CC %s" && %s %s %s -c %s -o %s\n' \
            "$(dirname "$o")" "$s" "$CC" "$CFLAGS" "$DEFS" "$s" "$o"
    done
} > "$OUT/objs.mk"
make -s -k -j"$JOBS" -f "$OUT/objs.mk" all

OBJS=
for s in $SRCS; do OBJS="$OBJS $OUT/obj/${s%.c}.o"; done
echo "LINK ZSNES.elf"
# shellcheck disable=SC2086
$CC -mhard-float -Wl,--gc-sections -o "$OUT/ZSNES.elf" $OBJS "$OUT/cfg.o" "$OUT/input.o" "$OUT/md.o" \
    -L"$PSL1GHT/ppu/lib" -L"$PS3DEV/portlibs/ppu/lib" \
    -laudio -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

# The PS3's executable: stripped, its system library imports linked
# (sprxlinker), and made a SELF signed for a homebrew package.
echo "SELF EBOOT.BIN"
ppu-strip "$OUT/ZSNES.elf" -o "$OUT/ZSNES.stripped.elf"
sprxlinker "$OUT/ZSNES.stripped.elf"
rm -rf "$OUT/pkg"
mkdir -p "$OUT/pkg/USRDIR"
make_self_npdrm "$OUT/ZSNES.stripped.elf" "$OUT/pkg/USRDIR/EBOOT.BIN" "$CONTENTID" >/dev/null
cp ps3/ICON0.PNG "$OUT/pkg/ICON0.PNG"
cp ps3/ReadMe.txt "$OUT/pkg/USRDIR/ReadMe.txt"
sfo --title "$TITLE" --appid "$APPID" -f "$PSL1GHT/bin/sfo.xml" "$OUT/pkg/PARAM.SFO"

echo "PKG zsnes-ps3.pkg"
rm -f "$OUT/zsnes-ps3.pkg"
pkg --contentid "$CONTENTID" "$OUT/pkg/" "$OUT/zsnes-ps3.pkg" >/dev/null
package_finalize "$OUT/zsnes-ps3.pkg"
ls -l "$OUT/pkg/USRDIR/EBOOT.BIN" "$OUT/zsnes-ps3.pkg"

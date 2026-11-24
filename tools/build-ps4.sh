#!/bin/sh
# build-ps4.sh - build ZSNES as PlayStation 4 homebrew with the OpenOrbis
# toolchain, as an installable .pkg for a PS4 running GoldHEN.
#
#   docker run --rm -v "$PWD":/w -w /w --user root openorbisofficial/toolchain:latest \
#       sh -c 'apt-get update && apt-get install -y python3 make && tools/build-ps4.sh'
#
# (.github/workflows/ps4.yml does that.) Leaves build-ps4/zsnes-ps4.pkg,
# which installs "ZSNES" (title ID ZSNS00001). OO_PS4_TOOLCHAIN is where
# OpenOrbis is (the image sets it). RIGHT_SPRX, if set, is the
# sce_sys/about/right.sprx to package (OpenOrbis's samples have one).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OO=${OO_PS4_TOOLCHAIN:?set OO_PS4_TOOLCHAIN to the OpenOrbis toolchain}
OUT=${OUT:-$ROOT/build-ps4}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
TITLE=ZSNES
TITLE_ID=ZSNS00001
CONTENT_ID=IV0000-${TITLE_ID}_00-ZSNESPS400000000
VERSION=1.00
CLANG=$(command -v clang || command -v clang-12)
LLD=$(command -v ld.lld || command -v ld.lld-12)
BIN=$OO/bin/linux

mkdir -p "$OUT"
cd "$ROOT"

# One word for the compiler: parsegen.py takes it that way.
cat > "$OUT/cc" <<EOF
#!/bin/sh
exec "$CLANG" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -isysroot "$OO" -isystem "$OO/include" "\$@"
EOF
chmod +x "$OUT/cc"
CC=$OUT/cc

# The core and the Unix frontend, as the Makefile lists them for an SDL 1.2
# build, with ps4/ps4.c for SDL and stand-ins for what the PS4 has no use for.
SRCS=$(make -s --no-print-directory --eval 'zsnes-print-srcs: ; @echo $(SRCS)' zsnes-print-srcs \
    ARCH=LINUX CPU=x86 BITS=64 WITH_SDL12=yes WITH_SDL=yes WITH_PNG= WITH_OPENGL= BUILD_DIR="$OUT/unused" 2>/dev/null |
    tr ' ' '\n' | grep -E '^[A-Za-z0-9_/.-]+\.c$' |
    grep -v -e '^unix/sdl12compat.c$' -e '^unix/sdl12gl.c$' -e '^unix/glupload.c$' -e '^unix/safelib.c$' \
        -e '^unix/net_transport.c$' | tr '\n' ' ')
SRCS="$SRCS ps4/ps4.c macos9/safelib.c macos9/net_stub.c"

DEFS="-DZSNES_PS4 -D__UNIXSDL__ -DZSNES_SDL12 -DNO_PNG -DNO_DEBUGGER -D_GNU_SOURCE"
CFLAGS="-std=gnu11 -O2 -fno-strict-aliasing -Wall -Wno-unused-parameter -Wno-unused-function \
 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-char-subscripts -Wno-pointer-sign \
 -Werror=implicit-function-declaration -I$OUT ${PS4_CFLAGS:-}"

for p in cfg input md; do
    if [ ! -f "$OUT/$p.o" ] || [ "$p.psr" -nt "$OUT/$p.o" ]; then
        echo "PSR $p"
        python3 ./parsegen.py $DEFS -gcc "$CC" -compile -flags "$CFLAGS $DEFS" \
            -cheader "$OUT/$p.h" -fname "$p" "$OUT/$p.o" "$p.psr"
    fi
done

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
"$LLD" -m elf_x86_64 -pie --script "$OO/link.x" --eh-frame-hdr -o "$OUT/ZSNES.elf" \
    $OBJS "$OUT/cfg.o" "$OUT/input.o" "$OUT/md.o" -L"$OO/lib" \
    -lc -lkernel -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService -lSceSystemService \
    "$OO/lib/crt1.o"

echo "FSELF eboot.bin"
PKG=$OUT/pkg
rm -rf "$PKG"
mkdir -p "$PKG/sce_sys/about"
"$BIN/create-fself" -in="$OUT/ZSNES.elf" -out="$OUT/ZSNES.oelf" --eboot "$PKG/eboot.bin" --paid 0x3800000000000011
cp ps4/icon0.png "$PKG/sce_sys/icon0.png"
FILES="eboot.bin sce_sys/param.sfo sce_sys/icon0.png"
if [ -n "${RIGHT_SPRX:-}" ] && [ -f "$RIGHT_SPRX" ]; then
    cp "$RIGHT_SPRX" "$PKG/sce_sys/about/right.sprx"
    FILES="$FILES sce_sys/about/right.sprx"
fi

echo "SFO param.sfo"
SFO=$PKG/sce_sys/param.sfo
"$BIN/PkgTool.Core" sfo_new "$SFO"
"$BIN/PkgTool.Core" sfo_setentry "$SFO" APP_TYPE --type Integer --maxsize 4 --value 1
"$BIN/PkgTool.Core" sfo_setentry "$SFO" APP_VER --type Utf8 --maxsize 8 --value "$VERSION"
"$BIN/PkgTool.Core" sfo_setentry "$SFO" ATTRIBUTE --type Integer --maxsize 4 --value 0
"$BIN/PkgTool.Core" sfo_setentry "$SFO" CATEGORY --type Utf8 --maxsize 4 --value gd
"$BIN/PkgTool.Core" sfo_setentry "$SFO" CONTENT_ID --type Utf8 --maxsize 48 --value "$CONTENT_ID"
"$BIN/PkgTool.Core" sfo_setentry "$SFO" DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"$BIN/PkgTool.Core" sfo_setentry "$SFO" SYSTEM_VER --type Integer --maxsize 4 --value 0
"$BIN/PkgTool.Core" sfo_setentry "$SFO" TITLE --type Utf8 --maxsize 128 --value "$TITLE"
"$BIN/PkgTool.Core" sfo_setentry "$SFO" TITLE_ID --type Utf8 --maxsize 12 --value "$TITLE_ID"
"$BIN/PkgTool.Core" sfo_setentry "$SFO" VERSION --type Utf8 --maxsize 8 --value "$VERSION"

echo "PKG zsnes-ps4.pkg"
(
    cd "$PKG"
    "$BIN/create-gp4" -out pkg.gp4 --content-id="$CONTENT_ID" --files "$FILES"
    "$BIN/PkgTool.Core" pkg_build pkg.gp4 .
)
mv "$PKG/$CONTENT_ID.pkg" "$OUT/zsnes-ps4.pkg"
ls -l "$PKG/eboot.bin" "$OUT/zsnes-ps4.pkg"

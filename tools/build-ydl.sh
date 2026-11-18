#!/bin/sh
# build-ydl.sh - cross-build zsnes for Yellow Dog Linux and other old 32-bit
# PowerPC Linux systems, from Debian or Ubuntu.
#
# The binary is linked against Debian 4.0 "etch" (glibc 2.3.6, SDL 1.2.11), so
# it needs nothing newer than GLIBC_2.3.4 and libSDL-1.2.so.0 at run time:
# Yellow Dog Linux 5, 6 and 7 all qualify. The packages come from
# archive.debian.org and are checked against the SHA1 sums in etch's index.
#
#   apt install -y gcc-powerpc-linux-gnu curl python3 make pkg-config
#   tools/build-ydl.sh [WORKDIR]          (default: ./build-ydl-sysroot)
#
# Leaves ./zsnes. Extra arguments for make go in MAKEARGS.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mkdir -p "${1:-$ROOT/build-ydl-sysroot}" && cd "${1:-$ROOT/build-ydl-sysroot}" && pwd)
MIRROR=https://archive.debian.org/debian
SYSROOT=$WORK/sysroot
BIN=$WORK/bin
CC=powerpc-linux-gnu-gcc
PKGS="libc6 libc6-dev linux-kernel-headers libsdl1.2debian-alsa libsdl1.2-dev zlib1g zlib1g-dev"

command -v $CC >/dev/null || { echo "need $CC (apt install gcc-powerpc-linux-gnu)" >&2; exit 1; }

# 1. The packages, verified against the index.
if [ ! -f "$SYSROOT/usr/include/SDL/SDL.h" ]; then
    mkdir -p "$WORK/debs"
    curl -fsS -o "$WORK/Packages.gz" "$MIRROR/dists/etch/main/binary-powerpc/Packages.gz"
    gzip -dc "$WORK/Packages.gz" | python3 -I -c '
import sys
want = set(sys.argv[1].split())
for blk in sys.stdin.read().split("\n\n"):
    f = dict(l.split(": ", 1) for l in blk.split("\n") if ": " in l and not l.startswith(" "))
    if f.get("Package") in want:
        print(f["SHA1"], f["Filename"])
' "$PKGS" > "$WORK/want.txt"
    : > "$WORK/sums.txt"
    while read -r sha fn; do
        deb=$WORK/debs/$(basename "$fn")
        [ -f "$deb" ] || curl -fsS -o "$deb" "$MIRROR/$fn"
        echo "$sha  $deb" >> "$WORK/sums.txt"
    done < "$WORK/want.txt"
    sha1sum -c --quiet "$WORK/sums.txt"
    rm -rf "$SYSROOT"
    mkdir -p "$SYSROOT"
    for deb in "$WORK"/debs/*.deb; do
        dpkg-deb -x "$deb" "$SYSROOT"
    done
    # Absolute symlinks (libm.so -> /lib/libm.so.6) would point at the build
    # machine; make them relative to the sysroot.
    (cd "$SYSROOT" && find . -type l | while read -r l; do
        t=$(readlink "$l")
        case "$t" in /*) ln -sfn "$(realpath -m --relative-to="$(dirname "$l")" ".$t")" "$l" ;; esac
    done)
fi

# 2. A compiler confined to the sysroot. The distribution's cross gcc searches
#    its own modern glibc first, so headers and libraries are named outright.
#    No stack protector (glibc 2.4) and 32-bit time_t/off_t (64-bit ones are
#    glibc 2.34). libgcc static: the old systems' libgcc_s is older than ours.
mkdir -p "$BIN"
GI=$($CC -print-file-name=include)
cat > "$BIN/ydl-gcc" <<EOF
#!/bin/sh
R=$SYSROOT
case " \$* " in
  *" -E "*|*" -c "*|*" -S "*|*" -M"*) LINK= ;;
  *) LINK="-B\$R/usr/lib/ -L\$R/usr/lib -L\$R/lib -Wl,-rpath-link,\$R/usr/lib:\$R/lib -Wl,--allow-shlib-undefined -static-libgcc" ;;
esac
exec $CC --sysroot=\$R -nostdinc -isystem $GI -isystem \$R/usr/include -fgnu89-inline -fno-stack-protector -U_TIME_BITS -U_FILE_OFFSET_BITS -D_FILE_OFFSET_BITS=32 \$LINK "\$@"
EOF
cat > "$BIN/ydl-sdl-config" <<EOF
#!/bin/sh
for a in "\$@"; do case "\$a" in
  --cflags) echo "-I$SYSROOT/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT" ;;
  --libs) echo "-L$SYSROOT/usr/lib -lSDL" ;;
  --version) echo 1.2.11 ;;
esac; done
EOF
cat > "$BIN/ydl-pkg-config" <<EOF
#!/bin/sh
PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig PKG_CONFIG_SYSROOT_DIR=$SYSROOT PKG_CONFIG_PATH= exec pkg-config "\$@"
EOF
chmod +x "$BIN"/ydl-*

# 3. The build. etch's libpng 1.2 drags zlib.h into every file and clashes
#    with miniz, so screenshots fall back to BMP.
cd "$ROOT"
# shellcheck disable=SC2086
make linux_ppc WITH_SDL12=yes WITH_PNG= BUILD_DIR=build-ydl \
    LINUX_PPC_CC="$BIN/ydl-gcc" LINUX_PPC_PKG_CONFIG="$BIN/ydl-pkg-config" \
    SDL_CONFIG="$BIN/ydl-sdl-config" ${MAKEARGS:-}

echo
echo "glibc symbol versions the binary needs (Yellow Dog Linux 5, 6 and 7 provide all of them):"
powerpc-linux-gnu-objdump -T zsnes | grep -o 'GLIBC_[0-9.]*' | sort -uV | tr '\n' ' '
echo

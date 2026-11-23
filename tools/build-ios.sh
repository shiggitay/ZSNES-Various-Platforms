#!/bin/sh
# build-ios.sh - build ZSNES for iOS and iPadOS (arm64, iOS 12 and later) as an
# unsigned .ipa, on a Mac with Xcode and GNU make (brew install make cmake).
#
#   tools/build-ios.sh            leaves build-ios/zsnes-ios.ipa
#
# The app is the SDL3 frontend with unix/ios.c added: an on-screen SNES
# controller, touches as the menus' pointer, game controllers as player 1,
# and the app's Documents folder (shown in the Files app) for ROMs and saves.
# Video goes through SDL's renderer (Metal); there is no desktop OpenGL on
# iOS, so unix/ios_glstub.c stands in for it. SDL3 is built here, statically.
#
# The .ipa is signed ad hoc only: sideload it with AltStore, SideStore or
# Sideloadly (which sign it with your Apple ID), or install it with
# TrollStore. MINIOS picks the oldest iOS it runs on.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-$ROOT/build-ios}
MINIOS=${MINIOS:-12.0}
SDL_TAG=${SDL_TAG:-release-3.4.14}
MAKE=${MAKE:-$(command -v gmake || echo make)}
JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}

SDK=$(xcrun --sdk iphoneos --show-sdk-path)
SDKVER=$(xcrun --sdk iphoneos --show-sdk-version)
CLANG=$(xcrun --sdk iphoneos -f clang)
mkdir -p "$OUT"
cd "$ROOT"

# One word for the compiler: parsegen.py takes it that way.
cat > "$OUT/cc" <<EOF
#!/bin/sh
exec "$CLANG" -isysroot "$SDK" -target arm64-apple-ios$MINIOS "\$@"
EOF
chmod +x "$OUT/cc"
CC=$OUT/cc

# 1. SDL3, static, for the device.
if [ ! -f "$OUT/sdl3-ios/lib/libSDL3.a" ]; then
    [ -d "$OUT/SDL" ] || git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git "$OUT/SDL"
    cmake -S "$OUT/SDL" -B "$OUT/SDL/build" \
        -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MINIOS" -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF \
        -DCMAKE_INSTALL_PREFIX="$OUT/sdl3-ios"
    cmake --build "$OUT/SDL/build" --parallel "$JOBS"
    cmake --install "$OUT/SDL/build"
fi

# 2. The sources: the Unix SDL3 build's, as the Makefile lists them, less what
# iOS has no use for or cannot have.
SRCS=$("$MAKE" -s --no-print-directory --eval 'zsnes-print-srcs: ; @echo $(SRCS)' zsnes-print-srcs \
    ARCH=LINUX CPU=arm64 WITH_SDL=yes WITH_OPENGL=yes WITH_PNG= WITH_AO= WITH_PIPEWIRE= \
    BUILD_DIR="$OUT/unused" |
    tr ' ' '\n' | grep -E '^[A-Za-z0-9_/.-]+\.c$' |
    grep -v -e '^unix/gl_draw.c$' -e '^unix/safelib.c$' -e '^unix/sdl_render.c$' | tr '\n' ' ')
[ -n "$SRCS" ] || { echo "could not list the sources (GNU make 3.82 or later is needed)" >&2; exit 1; }
# (The Makefile also prints notes on that line, such as finding no GL/gl.h for
# a Linux target here; only the .c files are kept.)
SRCS="$SRCS unix/sdl_render.c unix/ios.c unix/ios_glstub.c macos9/safelib.c"

DEFS="-DZSNES_IOS -D__UNIXSDL__ -D__OPENGL__ -DNO_PNG -DNO_DEBUGGER -D_GNU_SOURCE"
CFLAGS="-std=gnu11 -O2 -fno-strict-aliasing -Wall -Wno-unused-parameter -Wno-unused-function \
 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-deprecated-declarations \
 -Werror=implicit-function-declaration \
 -I$OUT -I$ROOT/unix/glcompat -I$OUT/sdl3-ios/include"

# 3. The configuration parsers, which parsegen.py writes and compiles.
for p in cfg input md; do
    if [ ! -f "$OUT/$p.o" ] || [ "$p.psr" -nt "$OUT/$p.o" ]; then
        echo "PSR $p"
        python3 ./parsegen.py $DEFS -gcc "$CC" -compile -flags "$CFLAGS $DEFS" \
            -cheader "$OUT/$p.h" -fname "$p" "$OUT/$p.o" "$p.psr"
    fi
done

# 4. Everything else, in parallel.
{
    printf 'all:'
    for s in $SRCS; do printf ' %s' "$OUT/obj/${s%.c}.o"; done
    printf '\n'
    for s in $SRCS; do
        o=$OUT/obj/${s%.c}.o
        extra=
        # SDL takes main() on iOS, to start the app's run loop first.
        [ "$s" = main.c ] && extra="-include SDL3/SDL_main.h"
        printf '%s: %s %s/cfg.h %s/input.h %s/md.h\n' "$o" "$s" "$OUT" "$OUT" "$OUT"
        printf '\t@mkdir -p %s && echo "CC %s" && %s %s %s %s -c %s -o %s\n' \
            "$(dirname "$o")" "$s" "$CC" "$CFLAGS" "$DEFS" "$extra" "$s" "$o"
    done
} > "$OUT/objs.mk"
"$MAKE" -s -k -j"$JOBS" -f "$OUT/objs.mk" all

# 5. The app.
APP=$OUT/Payload/ZSNES.app
rm -rf "$OUT/Payload"
mkdir -p "$APP"
OBJS=
for s in $SRCS; do OBJS="$OBJS $OUT/obj/${s%.c}.o"; done
SDL_LIBS=$(PKG_CONFIG_PATH="$OUT/sdl3-ios/lib/pkgconfig" pkg-config --static --libs sdl3)
echo "LINK ZSNES"
# shellcheck disable=SC2086
"$CC" -o "$APP/ZSNES" $OBJS "$OUT/cfg.o" "$OUT/input.o" "$OUT/md.o" $SDL_LIBS -lz -lm -dead_strip

sed -e "s/@MINIOS@/$MINIOS/" -e "s/@SDKVER@/$SDKVER/g" ios/Info.plist > "$APP/Info.plist"
xcrun ibtool --compile "$APP/LaunchScreen.storyboardc" ios/LaunchScreen.storyboard \
    --minimum-deployment-target "$MINIOS" --target-device iphone --target-device ipad
# The icon: an asset catalog of the one 1024-pixel image, which actool turns
# into every size the device asks for, and the Info.plist entries for them.
XCA=$OUT/Assets.xcassets
rm -rf "$XCA"
mkdir -p "$XCA/AppIcon.appiconset"
cp ios/AppIcon-1024.png "$XCA/AppIcon.appiconset/"
cat > "$XCA/Contents.json" <<'EOF'
{ "info" : { "author" : "xcode", "version" : 1 } }
EOF
cat > "$XCA/AppIcon.appiconset/Contents.json" <<'EOF'
{
  "images" : [
    { "filename" : "AppIcon-1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" }
  ],
  "info" : { "author" : "xcode", "version" : 1 }
}
EOF
xcrun actool "$XCA" --compile "$APP" --platform iphoneos --minimum-deployment-target "$MINIOS" \
    --app-icon AppIcon --output-partial-info-plist "$OUT/icon.plist" \
    --target-device iphone --target-device ipad --output-format human-readable-text
/usr/libexec/PlistBuddy -c "Merge $OUT/icon.plist" "$APP/Info.plist"
plutil -convert binary1 "$APP/Info.plist"
cp ios/ReadMe.txt "$OUT/ReadMe.txt" 2>/dev/null || true

# 6. Signed ad hoc, which TrollStore takes as it is and the sideloading
# tools replace with your own signature.
codesign --force --sign - --timestamp=none "$APP"
rm -f "$OUT/zsnes-ios.ipa"
(cd "$OUT" && zip -qr -X zsnes-ios.ipa Payload)
ls -l "$OUT/zsnes-ios.ipa"
lipo -info "$APP/ZSNES"
otool -l "$APP/ZSNES" | grep -A4 -E 'LC_BUILD_VERSION|LC_VERSION_MIN' | head -8

#!/bin/sh
# package-release.sh - make the release packages in builds/ from the builds
# that tools/build-ydl.sh and tools/build-macos9.sh leave:
#
#   builds/zsnes-ppc32-linux.tar.gz      zsnes-ppc32-linux/ with zsnes,
#                                        README.txt, zsnes.png and
#                                        install-launcher.sh (unix/ppc-linux)
#   builds/zsnes-ppc-classic-macos.sit.hqx
#                                        a "ZSNES PPC" folder with the
#                                        application and "ZSNES Read Me"
#                                        (macos9/ReadMe.txt), as StuffIt in
#                                        BinHex: StuffIt Expander opens it
#
# Then prints the SHA-256 sums for builds/README.md. Needs hfsutils (Retro68
# builds them; set RETRO68 as for build-macos9.sh) for the BinHex encoding.
#
#   RETRO68=/path/to/build/toolchain tools/package-release.sh
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ -n "${RETRO68:-}" ]; then PATH=$RETRO68/bin:$PATH; fi
LINUX=zsnes-ppc32-linux
MAC=zsnes-ppc-classic-macos

for f in "$ROOT/zsnes" "$ROOT/build-macos9/ZSNES.bin"; do
    [ -f "$f" ] || { echo "missing $f: build it first" >&2; exit 1; }
done
command -v hcopy >/dev/null || { echo "need hfsutils (hcopy): set RETRO68" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Linux
mkdir "$TMP/$LINUX"
cp "$ROOT/zsnes" "$ROOT/unix/ppc-linux/README.txt" "$ROOT/unix/ppc-linux/zsnes.png" \
    "$ROOT/unix/ppc-linux/install-launcher.sh" "$TMP/$LINUX/"
chmod 755 "$TMP/$LINUX/zsnes" "$TMP/$LINUX/install-launcher.sh"
chmod 644 "$TMP/$LINUX/README.txt" "$TMP/$LINUX/zsnes.png"
tar -C "$TMP" --owner=0 --group=0 -czf "$ROOT/builds/$LINUX.tar.gz" "$LINUX"

# Mac OS: a StuffIt archive of the folder, then BinHex of that, with the
# archive's type and creator, from an HFS volume hfsutils can read.
tr '\n' '\r' < "$ROOT/macos9/ReadMe.txt" > "$TMP/ReadMe.cr"
python3 "$ROOT/tools/mksit.py" "$TMP/ZSNES-PPC.sit" "ZSNES PPC" \
    "ZSNES=$ROOT/build-macos9/ZSNES.bin" "ZSNES Read Me=$TMP/ReadMe.cr:TEXT:ttxt"
dd if=/dev/zero of="$TMP/v.dsk" bs=1024 count=8192 2>/dev/null
(
    export HOME="$TMP"
    hformat -l Release "$TMP/v.dsk" >/dev/null
    hcopy -r "$TMP/ZSNES-PPC.sit" :ZSNES-PPC.sit
    hattrib -t 'SIT!' -c 'SIT!' :ZSNES-PPC.sit
    hcopy -b :ZSNES-PPC.sit "$ROOT/builds/$MAC.sit.hqx"
    humount >/dev/null
)

cd "$ROOT/builds"
ls -l "$MAC.sit.hqx" "$LINUX.tar.gz"
sha256sum "$MAC.sit.hqx" "$LINUX.tar.gz"

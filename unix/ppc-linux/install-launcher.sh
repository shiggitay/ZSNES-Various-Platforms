#!/bin/sh
# install-launcher.sh - put a ZSNES PPC launcher, with its icon, on the
# desktop and in the KDE and GNOME menus (Games). Run it from the folder
# you unpacked ZSNES into; run it again if you move that folder.
cd "$(dirname "$0")" || exit 1
DIR=$(pwd)

launcher() {
    mkdir -p "$(dirname "$1")" || return
    cat > "$1" <<DESKTOP
[Desktop Entry]
Type=Application
Encoding=UTF-8
Name=ZSNES PPC
GenericName=Super Nintendo Emulator
Comment=Play Super Nintendo games
Exec="$DIR/zsnes" %f
Path=$DIR
Icon=$DIR/zsnes.png
Terminal=false
Categories=Game;Emulator;
DESKTOP
    chmod 755 "$1"
    echo "  $1"
}

echo "ZSNES PPC launcher written to:"
launcher "$HOME/Desktop/zsnes-ppc.desktop"
# The freedesktop.org menu (GNOME 2, KDE 3.2 and later)
launcher "$HOME/.local/share/applications/zsnes-ppc.desktop"
# KDE 3.0 and 3.1's own menu
if [ -d "$HOME/.kde" ]; then
    launcher "$HOME/.kde/share/applnk/Games/zsnes-ppc.desktop"
fi
echo "The menu entry may appear after you log out and back in."

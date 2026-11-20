# ZSNES PPC builds

Ready-to-run builds of this repository. No ROMs are included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-macos9.tar.gz](zsnes-ppc-macos9.tar.gz) | Mac OS 8.6 to 9.2.2, PowerPC | `ZSNES.bin` (MacBinary, for StuffIt Expander), `ZSNES.dsk` (HFS disk image), `ReadMe.txt` |
| [zsnes-ppc-ydl.tar.gz](zsnes-ppc-ydl.tar.gz) | Yellow Dog Linux 3 and other 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt` |

Each package has a read me with requirements, keys and video modes.

**Mac OS 9:** needs 40 MB of free memory; drop a ROM on the icon. Video menu:
Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to
Monitor. OpenGL modes need Apple's OpenGL and an accelerated card. Tested on
a Power Mac 8500 with a G4/500 upgrade and a Radeon 7000.

**Yellow Dog Linux:** run `./zsnes game.smc` from a terminal in X. Ctrl-F
frame counter, Ctrl-Shift-F or Alt-Enter full screen. Plays straight to the
sound card, through aRts only when the card is busy. Tested on Yellow Dog
Linux 3.0.1 with kernel 2.4.22.

SHA-256:

```
a4dea028353185019ca498a0cd069354b799ec59f34289676624e62c65f87f14  zsnes-ppc-macos9.tar.gz
aa13b8fa441015cf87014e58cf627d772270febfcd24401964f115c22bb3d9f1  zsnes-ppc-ydl.tar.gz
```

Built from the commit before the one that added them, with
`tools/build-macos9.sh` and `tools/build-ydl.sh`.

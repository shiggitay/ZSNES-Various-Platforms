# ZSNES PPC builds

Ready-to-run builds of this repository, named by build number. No ROMs are
included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-macos9-build9.tar.gz](zsnes-ppc-macos9-build9.tar.gz) | Mac OS 8.6 to 9.2.2, PowerPC | `ZSNES.bin` (MacBinary, for StuffIt Expander), `ZSNES.dsk` (HFS disk image), `ReadMe.txt` |
| [zsnes-ppc-ydl-build8.tar.gz](zsnes-ppc-ydl-build8.tar.gz) | Yellow Dog Linux 3 and other 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt` |

Each package has a read me with requirements, keys and video modes.

**Mac OS 9:** needs 40 MB of free memory; drop a ROM on the icon. Video menu:
Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to
Monitor. OpenGL modes need Apple's OpenGL and an accelerated card. Tested on
a Power Mac 8500 with a G4/500 upgrade and a Radeon 7000.

**Yellow Dog Linux:** run `./zsnes game.smc` from a terminal in X. Ctrl-F
frame counter, Ctrl-Shift-F or Alt-Enter full screen. Plays straight to the
sound card, through aRts only when the card is busy. OpenGL modes with DRI
(the read me has the XF86Config that worked for a PCI Radeon 7000); full
screen keeps the desktop's resolution. Tested on Yellow Dog Linux 3.0.1 with
kernel 2.4.22.

SHA-256:

```
6e39df32d009c63105c6f80278a8f949addb373041b265f861b63f3590800958  zsnes-ppc-macos9-build9.tar.gz
f637313f2cf8a7cae579240bdc1d3b25aae10efb8250bc2f0fe8783d6c42ac35  zsnes-ppc-ydl-build8.tar.gz
```

Built from 5ecc39a with `tools/build-macos9.sh` and `tools/build-ydl.sh`.

ZSNES PPC 1.0: ZSNES 2 (the C port) for PowerPC Macs.

**Mac OS 8.6 to 9.2.2** - `zsnes-ppc-macos9-build10.tar.gz`
- `ZSNES.bin` (MacBinary, for StuffIt Expander) or `ZSNES.dsk` (HFS disk image).
- Needs 40 MB of free memory; drop a ROM on the icon or use Game > Load.
- OpenGL video modes (the O modes) through Apple's OpenGL with an accelerated card; full screen; Video menu: Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to Monitor. The Control Strip hides in full screen.

**Yellow Dog Linux 3 and other 32-bit PowerPC Linux** - `zsnes-ppc-ydl-build10.tar.gz`
- Needs only glibc 2.3 and SDL 1.2; Linux 2.4 or 2.6. Run from a terminal in X.
- Ctrl-F frame counter, Ctrl-Shift-F or Alt-Enter full screen.
- Hardware OpenGL modes through XFree86's DRI, full screen at the desktop's resolution; the read me has the XF86Config that worked for a PCI Radeon 7000.
- Plays straight to the sound card (OSS first), through aRts only when the card is busy.

Both tested on a Power Mac 8500 with a G4/500 upgrade and a Radeon 7000 (Mac OS 9; Yellow Dog Linux 3.0.1, kernel 2.4.22).

The emulator core gives the same machine state, picture and sound as on x86, frame for frame, on the test suite and 164 test ROMs; sprite, Mode 7 and background drawing are markedly faster than upstream's. No ROMs are included.

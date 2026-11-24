ZSNES PPC 1.0: ZSNES 2 (the C port) for PowerPC Macs (classic Mac OS, Mac OS X, Linux), and the iPhone.

**Mac OS 8.6 to 9.2.2** - `zsnes-ppc-classic-macos.sit.hqx`
- Double-click it: StuffIt Expander, included with Mac OS 8 and 9 (and what the browsers of the time hand downloads to), unpacks a "ZSNES PPC" folder with the application and its read me.
- Needs 40 MB of free memory; drop a ROM on the icon or use Game > Load.
- OpenGL video modes (the O modes) through Apple's OpenGL with an accelerated card; full screen; Video menu: Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to Monitor. The Control Strip hides in full screen.

**Mac OS X 10.2 to 10.4 (PowerPC)** - `zsnes-ppc-macosx.sit.hqx`
- A native Carbon application (not Classic), with the Mac OS 9 build's features: OpenGL video modes through Mac OS X's OpenGL, full screen with the menu bar and the Dock hidden, and the Video menu (Show FPS, Full Screen, Sync to Monitor).
- Double-click it: StuffIt Expander unpacks a "ZSNES PPC" folder with the application and its read me. Keep the folder somewhere you can write to.

**iPhone (iOS 12 and later)** - `zsnes-ios.ipa`
- Sideload it with AltStore, SideStore or Sideloadly (or TrollStore where it runs); ROMs go in ZSNES's folder in the Files app.
- An on-screen SNES controller beside the 4:3 picture, with MENU and FPS buttons. A Bluetooth controller works as player 1 straight away and hides the on-screen controls: a Switch Pro Controller by its letters, Xbox and PlayStation pads by position; Start+Select (+ and - on a Switch Pro) opens the ZSNES menu; there the left stick moves the pointer, A clicks or picks and B goes back.
- The battery save and settings are written whenever the app goes to the background.

**32-bit PowerPC Linux** - `zsnes-ppc32-linux.tar.gz`
- Needs only glibc 2.3 and SDL 1.2; Linux 2.4 or 2.6. Run from a terminal in X, or run `./install-launcher.sh` once for a desktop and menu launcher with the ZSNES icon; the window carries the icon too.
- Ctrl-F frame counter, Ctrl-Shift-F or Alt-Enter full screen.
- Hardware OpenGL modes through XFree86's DRI, full screen at the desktop's resolution; the read me has the XF86Config that worked for a PCI Radeon 7000.
- Plays straight to the sound card (OSS first), through aRts only when the card is busy.

Default controls: arrow keys for the d-pad, X = A, Z = B, S = X, A = Y, D/C = L/R, Return = Start, Right Shift = Select. Sound plays at 44100 Hz (CD quality), converted from the SNES's 32000 Hz by ZSNES's own filter rather than the sound driver's.

Both tested on a Power Mac 8500 with a Buffalo G4/500 MHz upgrade and a Radeon 7000 (Mac OS 9; Yellow Dog Linux 3.0.1, kernel 2.4.22).

The emulator core gives the same machine state, picture and sound as on x86, frame for frame, on the test suite and 164 test ROMs; sprite, Mode 7 and background drawing are markedly faster than upstream's. No ROMs are included.

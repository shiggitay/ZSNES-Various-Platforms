ZSNES PPC 1.0 for Yellow Dog Linux 3 (PowerPC)

Run it from a terminal in X (ZSNES will not start without an X display):
    ./zsnes "/path/to/game.smc"

KEYS
    Esc             ZSNES menu
    Ctrl-F          frame counter on/off
    Ctrl-Shift-F    window <-> full screen (Alt-Enter does the same)

VIDEO MODES (Config > Video)
Modes with an O use OpenGL: the graphics card scales the picture.
- 512x448 ODR W, 768x672 ODR W ...: fixed windows.
- VARIABLE ODR W / ODS W: a window you can resize to any size.
- 640x480 ODS F, 800x600 ODS F, 1024x768 ODS F ...: full screen at that
  resolution.
They need OpenGL drawn by the graphics card (XFree86's DRI). Full screen
in an O mode keeps the desktop's resolution and the card stretches the
picture to it.
On Yellow Dog Linux 3 with a PCI Radeon 7000 in a Power Mac 8500, DRI
worked with these in /etc/X11/XF86Config (and "modprobe radeon" at boot):
    Section "Module":  Load "dri"
    Section "Device":  BusID "PCI:0:14:0"   (your card's, from the X log)
                       Option "UseFBDev" "true"
                       Option "ForcePCIMode" "true"
    Section "Screen":  DefaultDepth 16, and Depth 16 in "Display"
                       (15 and 24 garbled the whole desktop)
    Section "DRI":     Mode 0666
Check with: glxinfo | grep "direct rendering"  (Yes, Mesa DRI Radeon).
If full screen in an O mode stays black, try it as a borderless window the
size of the desktop instead:
    ZSNES_SDL12_GLFS=window ./zsnes ...
With Mesa's hardware drivers on PowerPC the picture goes to the card as
RGBA bytes, since those drivers read packed 16-bit pixels byte-swapped; if
the colours of an O mode are wrong, try:
    ZSNES_GL_UPLOAD=565 ./zsnes ...   (or ZSNES_GL_UPLOAD=rgba)
If OpenGL is missing, or only Mesa's software renderer is there, ZSNES says
so in the terminal and the O modes fall back to the 512x448 window; its own
software modes are faster than software OpenGL. To try them anyway:
    ZSNES_SDL12_SOFTGL=1 ./zsnes ...
The OpenGL modes do not wait for the monitor's refresh (that halves the
frame rate on a monitor not running at 60 Hz). To wait anyway, on an SDL of
1.2.10 or later:
    ZSNES_SDL12_VSYNC=1 ./zsnes ...

SOUND
ZSNES plays straight to the sound card when it can (OSS /dev/dsp, then
ALSA), and through a sound server (aRts, ESD) only when the card is
busy; it says in the terminal which it used. On Yellow Dog Linux 3, aRts
stuttered under ZSNES's load, so to keep it off the card while you play:
    artsshell suspend
before starting ZSNES (artsd keeps running and takes the card back when a
KDE program next plays something), or in KDE Control Center > Sound System
turn on "Auto-suspend if idle". SDL_AUDIODRIVER=arts forces aRts anyway.
If aRts fails with "loading the aRts backend /usr/lib/libartscbackend.la
failed", that file is missing; recreating it (it describes
libartscbackend.so.0) fixes aRts for every program.

ALSO
- Fixed (ZSNES PPC 1.0): on the 2.4 kernel, no snow in the menu and a black
  screen in games. Linux 2.4 lacks the clock ZSNES timed its frames with,
  so time stood still; it now falls back to the ordinary clock.
- Fixed: the hang after the first frame in full screen (ZSNES no longer
  asks X to grab the mouse pointer).
- Faster sprite, Mode 7 and background drawing.
- Sound falls back to any SDL driver that works (ALSA, OSS, ESD, aRts).

ABOUT ZSNES PPC
ZSNES PPC is ZSNES for PowerPC: a fork of ZSNES 2, Alexander F. Rodseth's C
port of ZSNES (github.com/xyproto/zsnes), built for Yellow Dog Linux 3 and
other 32-bit PowerPC Linux (Linux 2.4 or 2.6, glibc 2.3, SDL 1.2). The
emulator core gives the same machine state, picture and sound as on an x86
PC, frame for frame, and its sprite, Mode 7 and background drawing were made
faster for older machines. No ROMs are included.

Source code, other builds and problem reports:
    https://github.com/shiggitay/zsnes-ppc

CREDITS
ZSNES by zsKnight, _Demo_, pagefault, Nach, grinvader, Deathlike2, Jonas
Quinn, blargg, Pharos, teuf, relnev, prometheus, theoddone33, EvilTypeGuy,
stainless, aaronl, MKendora, kode54, zinx, amit, hpsolo, Kreed, Neviksti,
ipher, StatMat, Diablo-D3, Wilbern Cobb, Thorsten "mirabile" Glaser and
Mitchell "The Khan Artist/Noxious Ninja" Mebane. The C port by Christoph
Mallon and Alexander F. Rodseth.

ZSNES is free software under the GNU General Public License, version 2.

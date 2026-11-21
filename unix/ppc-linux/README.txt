ZSNES PPC 1.0 for 32-bit PowerPC Linux

WHAT YOU NEED
- A PowerPC Mac or other 32-bit PowerPC machine (G3 or G4 recommended)
  running Linux 2.4 or 2.6 with glibc 2.3 or newer, X and SDL 1.2.
- Tested on Yellow Dog Linux 3.0.1 (kernel 2.4.22) on a Power Mac 8500 with
  a Buffalo G4/500 MHz upgrade card and an ATI Radeon 7000.

GETTING STARTED
1. Unpack the archive where you want to keep ZSNES:
       tar xzf zsnes-ppc32-linux.tar.gz
2. Run it from a terminal in X (ZSNES will not start without an X display):
       cd zsnes-ppc32-linux
       ./zsnes "/path/to/game.smc"
   or without a ROM, and pick one in its menu (Game > Load).
3. For a launcher with the ZSNES icon on the desktop and in the KDE or
   GNOME menu, run once:
       ./install-launcher.sh
   It points at this folder, so run it again if you move the folder. A ROM
   dropped on the desktop launcher starts ZSNES with that game.
Settings are kept in ~/.config/zsnes (or in ~/.zsnes, if you have one
from an older ZSNES).

CONTROLS (player 1, as they come)
    D-pad           the arrow keys
    A               X
    B               Z
    X               S
    Y               A
    L / R           D / C
    Start           Return
    Select          Right Shift
Change them in Config > Input.

KEYS
    Esc             ZSNES menu
    Ctrl-F          frame counter on/off
    Ctrl-Shift-F    window <-> full screen (Alt-Enter does the same)

VIDEO MODES (Config > Video)
Modes with an O use OpenGL: the graphics card scales the picture.
- 512x448 ODR W, 768x672 ODR W ...: fixed windows.
- VARIABLE ODR W / ODS W: a window you can resize to any size.
- 640x480 ODS F, 800x600 ODS F, 1024x768 ODS F ...: full screen. Full
  screen keeps the desktop's resolution and the card stretches the picture
  to it.
They need OpenGL drawn by the graphics card: XFree86's or X.Org's DRI.
Check with:
    glxinfo | grep "direct rendering"       (it should say Yes)
For example, with a PCI Radeon 7000 in a Power Mac 8500 under XFree86 4.3,
DRI worked with these in /etc/X11/XF86Config (and "modprobe radeon" before
X starts):
    Section "Module":  Load "dri"
    Section "Device":  BusID "PCI:0:14:0"   (your card's, from the X log)
                       Option "UseFBDev" "true"
                       Option "ForcePCIMode" "true"
    Section "Screen":  DefaultDepth 16, and Depth 16 in "Display"
                       (15 and 24 garbled the whole desktop)
    Section "DRI":     Mode 0666
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
ZSNES plays at 44100 Hz, CD quality. The SNES makes its sound at 32000 Hz;
ZSNES converts it with a high-quality filter rather than leaving that to
the sound driver, which does it coarsely. ZSNES_AUDIO_RATE=48000 (or
another rate) opens the card at that rate instead.
ZSNES plays straight to the sound card when it can (OSS /dev/dsp, then
ALSA), and through a sound server (aRts, ESD) only when the card is busy;
it says in the terminal which it used. KDE 3's aRts can stutter under
ZSNES's load; to keep it off the card while you play:
    artsshell suspend
before starting ZSNES (artsd keeps running and takes the card back when a
KDE program next plays something), or in KDE Control Center > Sound System
turn on "Auto-suspend if idle". SDL_AUDIODRIVER=arts forces aRts anyway.
If aRts fails with "loading the aRts backend /usr/lib/libartscbackend.la
failed", that file is missing; recreating it (it describes
libartscbackend.so.0) fixes aRts for every program.

NOTES
- Linux 2.4 lacks the clock ZSNES times its frames with; ZSNES falls back
  to the ordinary clock there.
- ZSNES does not grab the mouse pointer in full screen (on some X servers
  of the era that hung it after the first frame).

ABOUT ZSNES PPC
ZSNES PPC is ZSNES for PowerPC: a fork of ZSNES 2, Alexander F. Rodseth's C
port of ZSNES (github.com/xyproto/zsnes), built for 32-bit PowerPC Linux.
The emulator core gives the same machine state, picture and sound as on an
x86 PC, frame for frame, and its sprite, Mode 7 and background drawing were
made faster for older machines. No ROMs are included.

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

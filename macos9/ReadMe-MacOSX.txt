ZSNES PPC 1.0 for Mac OS X (PowerPC)

WHAT YOU NEED
- A PowerPC Mac (G3 or G4 recommended) with Mac OS X 10.2 to 10.4. It is
  a Carbon application: Mac OS X runs it natively, not in Classic. Mac OS X
  10.1 runs it too, without hiding the Dock in full screen.

GETTING STARTED
1. Copy the "ZSNES PPC" folder somewhere you can write to, such as your
   Applications folder or your home folder. ZSNES keeps its settings,
   battery saves (.srm) and save states (.zst) in the folder it is in.
2. Drop a ROM file onto the ZSNES icon, or open ZSNES and pick a ROM in its
   menu (Game > Load). In the file list, "/" shows your disks, and ".."
   goes up a folder.
3. Esc opens and closes the ZSNES menu. ZSNES PPC > Quit (Command-Q) quits.

CONTROLS (player 1, as they come)
    D-pad           the arrow keys
    A               X
    B               Z
    X               S
    Y               A
    L / R           D / C
    Start           Return
    Select          Right Shift
Change them in Config > Input. A Control-click is a right click.

VIDEO MODES AND SPEED
Pick a mode in Config > Video. Modes with an O use OpenGL: the graphics card
scales the picture, which takes most of the drawing work off the processor.
They need a graphics card Mac OS X accelerates OpenGL on (Rage 128, Radeon,
GeForce); without one those modes fall back to the 512x448 window.
- Fastest full screen: 640x480 ODS F (OpenGL), or 256x224 R F without
  OpenGL, which ZSNES doubles when the screen has room.
- Full screen hides the menu bar and the Dock, and brings both back when
  you leave it or switch to another application (Command-Tab).
- Full screen covers the desktop at its own resolution. In the OpenGL modes
  the graphics card scales the picture to fill it, which costs it little.
- VARIABLE (OpenGL) windows can be resized from their corner and zoomed.
- Video > Show FPS (Command-F) toggles the frame counter.
- Shift-Command-F, Command-Return, Option-Return or Video > Full Screen switch
  between your last windowed and your last full-screen mode.
- Video > Sync to Monitor (OpenGL modes) waits for the screen's refresh:
  no tearing, but on a monitor not running at 60 Hz it costs frames. It is
  off each time ZSNES starts; ZSNES's own VSync setting has no effect here.
- The FPS counter shows frames drawn. When the Mac falls behind, ZSNES
  skips drawing some frames to keep the game at full speed, so the counter
  can read under 60 while the game itself runs at the right pace.
- Thousands of colors (System Preferences > Displays) is faster than
  Millions outside the OpenGL modes.

SOUND
ZSNES plays at 44100 Hz, CD quality. The SNES makes its sound at 32000 Hz;
ZSNES converts it with a high-quality filter rather than leaving that to
the system, which does it coarsely.

NAMES
ZSNES opens files the way Mac OS 9 programs do, which allows 31 characters
in a file name: a ROM with a longer name shows up shortened, with a # and a
number in it. Rename such ROMs to 31 characters or fewer first; the battery
save and save states take the ROM's name.

WHAT IS NOT THERE YET
- Joysticks and gamepads.
- Netplay.
- Movie recording to video files and PNG screenshots (screenshots are BMP).
- Smooth (bilinear) scaling outside the OpenGL modes.

A log of what ZSNES printed is in "ZSNES Log.txt" next to the application;
include it with any problem report.

ABOUT ZSNES PPC
ZSNES PPC is ZSNES for PowerPC: a fork of ZSNES 2, Alexander F. Rodseth's C
port of ZSNES (github.com/xyproto/zsnes), as a native Carbon application
for Mac OS X on PowerPC. The emulator core gives the same machine state,
picture and sound as on an x86 PC, frame for frame, and its sprite, Mode 7
and background drawing were made faster for older machines. No ROMs are
included.

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

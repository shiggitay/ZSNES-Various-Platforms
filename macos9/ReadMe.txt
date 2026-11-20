ZSNES PPC 1.0 for Mac OS 8.6 - 9.2.2 (PowerPC)

WHAT YOU NEED
- A PowerPC Mac (G3 or G4 recommended) with Mac OS 8.6 to 9.2.2. It runs
  in the Classic environment of Mac OS X too.
- At least 40 MB of free memory. The application asks for 64 MB; lower it
  in the Finder's Get Info window (Memory) if you have less.

GETTING STARTED
1. Copy ZSNES to a folder of its own on your hard disk. Its settings,
   battery saves (.srm) and save states (.zst) are kept in that folder.
2. Drop a ROM file onto the ZSNES icon, or open ZSNES and pick a ROM in its
   menu (Game > Load). In the file list, "/" shows your disks, and ".."
   goes up a folder.
3. Esc opens and closes the ZSNES menu. Command-Q quits.

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
They need Apple's OpenGL extensions and an accelerated card (Rage 128,
Radeon, GeForce); without them those modes fall back to 512x448 window.
- Fastest full screen: 640x480 ODS F (OpenGL), or 256x224 R F without
  OpenGL, which ZSNES doubles when the screen has room.
- Full screen hides the menu bar and the Control Strip, and brings both
  back when you leave it or switch to another application.
- Full screen covers the desktop at its own resolution. In the OpenGL modes
  the graphics card scales the picture to fill it, which costs it little.
- VARIABLE (OpenGL) windows can be resized with the grow box and zoomed.
- Video > Show FPS (Command-F) toggles the frame counter.
- Shift-Command-F, Command-Return, Option-Return or Video > Full Screen switch
  between your last windowed and your last full-screen mode.
- Video > Sync to Monitor (OpenGL modes) waits for the screen's refresh:
  no tearing, but on a monitor not running at 60 Hz it costs frames. It is
  off each time ZSNES starts; ZSNES's own VSync setting has no effect here.
- The FPS counter shows frames drawn. When the Mac falls behind, ZSNES
  skips drawing some frames to keep the game at full speed, so the counter
  can read under 60 while the game itself runs at the right pace.
- Thousands of colors (16-bit) is faster than Millions.

SOUND
ZSNES plays at 44100 Hz, CD quality. The SNES makes its sound at 32000 Hz;
ZSNES converts it with a high-quality filter rather than leaving that to
the Sound Manager, which does it coarsely.

NAMES
Mac OS 8 and 9 allow 31 characters in a file name. A ROM with a longer name
(from a PC or a Mac OS X disk) should be renamed first; the battery save
and save states take the ROM's name.

WHAT IS NOT THERE YET
- Joysticks and gamepads (InputSprocket support would be the way).
- Netplay (it needs Open Transport).
- Movie recording to video files and PNG screenshots (screenshots are BMP).
- Smooth (bilinear) scaling outside the OpenGL modes.

A log of what ZSNES printed is in "ZSNES Log.txt" next to the application;
include it with any problem report.

ABOUT ZSNES PPC
ZSNES PPC is ZSNES for PowerPC: a fork of ZSNES 2, Alexander F. Rodseth's C
port of ZSNES (github.com/xyproto/zsnes), as a native application for Mac OS
8.6 to 9.2.2. The emulator core gives the same machine state, picture and
sound as on an x86 PC, frame for frame, and its sprite, Mode 7 and
background drawing were made faster for older machines. No ROMs are
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

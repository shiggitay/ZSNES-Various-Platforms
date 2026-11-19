ZSNES 2 for Mac OS 8 and 9 (PowerPC)

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

Default keys for player 1: the arrow keys, Return (Start), Right Shift
(Select), Z (B), X (A), A (Y), S (X), D (L) and C (R). Change them in
Config > Input. A Control-click is a right click.

VIDEO MODES AND SPEED
Pick a mode in Config > Video. Modes with an O use OpenGL: the graphics card
scales the picture, which takes most of the drawing work off the processor.
They need Apple's OpenGL extensions and an accelerated card (Rage 128,
Radeon, GeForce); without them those modes fall back to 512x448 window.
- Fastest full screen: 640x480 ODS F (OpenGL), or 256x224 R F without
  OpenGL, which ZSNES doubles when the screen has room.
- Full screen covers the desktop at its own resolution. In the OpenGL modes
  the graphics card scales the picture to fill it, which costs it little.
- VARIABLE (OpenGL) windows can be resized with the grow box and zoomed.
- Video > Show FPS (Command-F) toggles the frame counter.
- Command-Return (or Option-Return, or Video > Toggle Full Screen) switches
  between your last windowed and your last full-screen mode.
- Thousands of colors (16-bit) is faster than Millions.

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

ZSNES is free software under the GNU General Public License, version 2.

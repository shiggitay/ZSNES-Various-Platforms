ZSNES for PS4

WHAT YOU NEED
- A PlayStation 4 running GoldHEN (on a firmware GoldHEN supports), so it
  can install and run homebrew.

GETTING STARTED
1. Copy zsnes-ps4.pkg to a USB drive (exFAT or FAT32), plug it in, and
   install it with GoldHEN's Package Installer (Settings > Debug Settings >
   Game > Package Installer, once the debug settings are on). ZSNES appears
   on the home screen.
2. Put your ROMs (.sfc, .smc, .zip) in /data/zsnes/roms on the PS4, for
   example with GoldHEN's FTP server, or on a USB drive (/mnt/usb0).
3. Start ZSNES and pick a ROM with Game > Load. The file list starts in
   /data/zsnes; "/" shows the whole file system. Settings, battery saves
   (.srm) and save states (.zst) are kept in /data/zsnes or beside the ROM.

CONTROLS
DualShock 4, in a game:
    D-pad or left stick    the SNES's d-pad
    Circle, Cross          A, B
    Triangle, Square       X, Y     (the SNES pad's own places)
    L1 or L2, R1 or R2     L, R
    Options                Start
    Touch pad (click)      Select
    Options + touch pad    the ZSNES menu (R3, pressing the right stick,
                           too)
    L3 (press the stick)   the frame counter on or off
The pads of the second, third and fourth users logged in play players 2,
3 and 4, with no setting up. Config > Input changes any of them.

In the ZSNES menus, with pad 1:
    Left stick             moves the pointer
    Cross                  clicks where the pointer is, after the stick
                           moved it; picks what is highlighted, after the
                           d-pad did
    Circle                 back
    D-pad                  moves the highlight
Options and the touch pad do nothing in the menus.

VIDEO AND SOUND
The picture is 4:3, as tall as the screen, in 1080p (the PS4 scales it for
a 720p TV), and changes on the TV's vertical blank: no tearing. Config >
Video picks the size ZSNES draws at (256x224, or 512x448 for the filters).
Sound plays at 48000 Hz, converted from the SNES's 32000 Hz by ZSNES's own
filter.

IF IT CRASHES
ZSNES writes what it is doing to "ZSNES Log.txt" (and its own messages to
"ZSNES Output.txt") in /data/zsnes; fetch them over FTP after a crash and
include them with the report: the last lines say how far it got.

WHAT IS NOT THERE YET
- A USB keyboard, netplay, movie recording and PNG screenshots
  (screenshots are BMP).
- Closing ZSNES from the PS4's own menu (PS button) does not save the
  game's battery save; quit from ZSNES's menu (Game > Quit) to keep it, or
  turn on Config > Saves > SRAM Check+Save, which saves it every few
  seconds.

ABOUT ZSNES FOR PS4
ZSNES for PS4 comes from ZSNES PPC, a fork of ZSNES 2, Alexander F.
Rodseth's C port of ZSNES (github.com/xyproto/zsnes), here as a native
program for the PlayStation 4, made with the OpenOrbis toolchain. No ROMs
are included.

Source code, other builds and problem reports:
    https://github.com/shiggitay/zsnes-ppc

CREDITS
ZSNES by zsKnight, _Demo_, pagefault, Nach, grinvader, Deathlike2, Jonas
Quinn, blargg, Pharos, teuf, relnev, prometheus, theoddone33, EvilTypeGuy,
stainless, aaronl, MKendora, kode54, zinx, amit, hpsolo, Kreed, Neviksti,
ipher, StatMat, Diablo-D3, Wilbern Cobb, Thorsten "mirabile" Glaser and
Mitchell "The Khan Artist/Noxious Ninja" Mebane. The C port by Christoph
Mallon and Alexander F. Rodseth. PS4 homebrew made with the OpenOrbis PS4
toolchain.

ZSNES is free software under the GNU General Public License, version 2.

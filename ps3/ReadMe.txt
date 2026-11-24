ZSNES for PS3

WHAT YOU NEED
- A PlayStation 3 that runs homebrew: custom firmware (CFW) or PS3HEN.
  ZSNES for PS3 is a native PS3 program, run from the XMB; it runs on the
  Cell's PowerPC core (the PPU).

GETTING STARTED
1. Copy zsnes-ps3.pkg to a USB drive, plug it in, and install it with
   Package Manager > Install Package Files (on HEN, enable HEN first).
   ZSNES appears under Game in the XMB.
2. Put your ROMs (.sfc, .smc, .zip) on a USB drive or on the PS3's hard
   disk, for example with FTP (multiMAN, webMAN) into /dev_hdd0/ROMS.
3. Start ZSNES and pick a ROM with Game > Load. The file list starts in
   the program's own folder (/dev_hdd0/game/ZSNE00001/USRDIR); ".." goes
   up, and "/" shows the drives: dev_hdd0 is the hard disk, dev_usb000 and
   so on the USB drives. Settings, battery saves (.srm) and save states
   (.zst) are kept in the program's folder or beside the ROM.

CONTROLS
DualShock 3 (or Sixaxis, or any pad the PS3 takes), in a game:
    D-pad or left stick    the SNES's d-pad
    Circle, Cross          A, B
    Triangle, Square       X, Y     (the SNES pad's own places)
    L1 or L2, R1 or R2     L, R
    Start, Select          Start, Select
    Start + Select         the ZSNES menu
    L3 (press the stick)   the frame counter on or off
A second, third and fourth pad play players 2, 3 and 4, with no setting
up. Config > Input changes any of them.

In the ZSNES menus, with pad 1:
    Left stick             moves the pointer
    Cross                  clicks where the pointer is, after the stick
                           moved it; picks what is highlighted, after the
                           d-pad did
    Circle                 back
    D-pad                  moves the highlight
Start and Select do nothing in the menus.

A USB keyboard works as on a computer: the arrow keys, X = A, Z = B,
S = X, A = Y, D/C = L/R, Return = Start, Right Shift = Select, Esc for the
menu, Ctrl-F for the frame counter.

VIDEO AND SOUND
The picture is 4:3, as tall as the screen, at the resolution the PS3 is
set to (720p, 1080p, ...): the RSX scales it, which costs the Cell nothing.
Config > Video picks the size ZSNES draws at (256x224, or 512x448 for the
filters) and Bilinear Filtering for a smoother picture; VSync follows the
TV. Sound plays at 48000 Hz, converted from the SNES's 32000 Hz by ZSNES's
own filter.

WHAT IS NOT THERE YET
- Netplay, movie recording to video files and PNG screenshots
  (screenshots are BMP).
- The PS button opens the XMB's own menu, as in any PS3 game; quitting
  from it saves the game's battery save as quitting from ZSNES does.

ABOUT ZSNES FOR PS3
ZSNES for PS3 comes from ZSNES PPC, ZSNES for PowerPC: a fork of ZSNES 2,
Alexander F. Rodseth's C port of ZSNES (github.com/xyproto/zsnes), here as a
native program for the PlayStation 3's Cell processor, made with PSL1GHT.
The emulator core gives the same machine state, picture and sound as on an
x86 PC, frame for frame. No ROMs are included.

Source code, other builds and problem reports:
    https://github.com/shiggitay/zsnes-ppc

CREDITS
ZSNES by zsKnight, _Demo_, pagefault, Nach, grinvader, Deathlike2, Jonas
Quinn, blargg, Pharos, teuf, relnev, prometheus, theoddone33, EvilTypeGuy,
stainless, aaronl, MKendora, kode54, zinx, amit, hpsolo, Kreed, Neviksti,
ipher, StatMat, Diablo-D3, Wilbern Cobb, Thorsten "mirabile" Glaser and
Mitchell "The Khan Artist/Noxious Ninja" Mebane. The C port by Christoph
Mallon and Alexander F. Rodseth. PS3 homebrew made with PSL1GHT and the
ps3dev toolchain.

ZSNES is free software under the GNU General Public License, version 2.

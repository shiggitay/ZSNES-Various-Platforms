ZSNES PPC 1.0 for iPhone

WHAT YOU NEED
- An iPhone with iOS 12 or later: the iPhone 5s and every iPhone since
  (the iPod touch 6th and 7th generation too). It is laid out for the
  iPhone's screen, the 16:9 ones (iPhone 6 to 8, SE) and the taller ones
  (iPhone X and later) alike; an iPad runs it in an iPhone-sized window.
- A way to install an app that is not from the App Store. zsnes-ios.ipa is
  not signed for any one person's device, so one of these does that:
    AltStore or SideStore (altstore.io, sidestore.io) - sign with your
    Apple ID; a free Apple ID's apps run for 7 days, then are refreshed.
    Sideloadly (sideloadly.io) - the same, from a Mac or PC.
    TrollStore, on the iOS versions it supports - installs it for good.
  On iOS 16 and later, turn on Developer Mode (Settings > Privacy &
  Security) when iOS asks, and trust your Apple ID under Settings >
  General > VPN & Device Management.

GETTING STARTED
1. Install zsnes-ios.ipa with one of the tools above, and open ZSNES once.
2. Put your ROMs (.sfc, .smc, .zip) in ZSNES's folder: in the Files app,
   On My iPhone > ZSNES. From a computer, the Finder (macOS
   10.15 and later) or iTunes shows the same folder under File Sharing.
3. In ZSNES, tap Game > Load and pick the ROM. Battery saves (.srm) and
   save states (.zst) go next to it, in the same folder.

CONTROLS
On screen, while a game runs:
    D-pad              left thumb (slide it for diagonals)
    A, B, X, Y         right thumb, in the SNES's own places
    L, R               the top corners
    Select, Start      the bottom corners
    MENU               top middle: the ZSNES menu, and back to the game
    FPS                beside it: the frame counter on or off
In the menus a tap is a click. The buttons press player 1's keys, so a
change in Config > Input applies to them too.

A Bluetooth game controller works as player 1 as soon as it is connected,
and the on-screen controls hide until it is disconnected:
- Nintendo Switch Pro Controller (iOS 16 and later): A, B, X and Y are the
  SNES's own, letters and places alike; L and R are L and R; + is Start,
  - is Select.
- Xbox, PlayStation and MFi controllers: the face buttons by position, as
  on a SNES pad (bottom B, right A, left Y, top X); the shoulder buttons
  are L and R; Menu/Options is Start, View/Share is Select.
The d-pad or the left stick moves. Start and Select together (+ and - on a
Switch Pro Controller) open the ZSNES menu; so does the Home button, where
iOS lets it through, and a tap at the top middle of the screen.

In the ZSNES menus, with a controller connected:
    Left stick         moves the pointer
    A                  clicks where the pointer is, after the stick moved
                       it; picks what is highlighted, after the d-pad did
    B                  back
    D-pad              moves the highlight
Start and Select do nothing there: together they only summon the menu
during a game, and B leaves it. Without a controller,
taps and keys work in the menus as before.

VIDEO AND SOUND
The picture is drawn 4:3, as tall as the screen, by the graphics processor
(Metal), with the controls in the margins beside it (over its edges a little
on a 16:9 iPhone, where the margins are narrower); the ZSNES filters (Config > Video) work as elsewhere. Sound plays
at 44100 Hz, and also with the ring/silent switch on silent.

WHEN ZSNES GOES TO THE BACKGROUND
iOS may close apps in the background. ZSNES writes the game's battery save
and its settings each time it goes there, so nothing is lost, but use save
states for anything else you want to keep.

WHAT IS NOT THERE YET
- Portrait orientation (ZSNES turns to landscape).
- Opening a ROM from another app ("Open in"); put it in the ZSNES folder.
- Netplay, movie recording and PNG screenshots (screenshots are BMP).

ABOUT ZSNES PPC
ZSNES PPC is a fork of ZSNES 2, Alexander F. Rodseth's C port of ZSNES
(github.com/xyproto/zsnes), made for PowerPC Macs and carried over to iOS.
No ROMs are included.

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

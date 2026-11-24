# ZSNES PPC builds

Ready-to-run builds of this repository. No ROMs are included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-classic-macos.sit.hqx](zsnes-ppc-classic-macos.sit.hqx) | Mac OS 8.6 to 9.2.2, PowerPC | double-click it: StuffIt Expander (included with Mac OS 8 and 9) gives you a "ZSNES PPC" folder with the application and its read me |
| [zsnes-ppc-macosx.sit.hqx](zsnes-ppc-macosx.sit.hqx) | Mac OS X 10.2 to 10.4, PowerPC (native Carbon application) | double-click it: StuffIt Expander (included with Mac OS X) gives you a "ZSNES PPC" folder with the application and its read me |
| [zsnes-ps4.pkg](zsnes-ps4.pkg) | PlayStation 4 with GoldHEN | install with GoldHEN's Package Installer; ZSNES appears on the home screen; see [ps4/ReadMe.txt](../ps4/ReadMe.txt) |
| [zsnes-ps3.pkg](zsnes-ps3.pkg) | PlayStation 3 with custom firmware or HEN | install with Package Manager; ZSNES appears under Game; see [ps3/ReadMe.txt](../ps3/ReadMe.txt) |
| [zsnes-ios.ipa](zsnes-ios.ipa) | iPhone, iOS 12 and later (arm64) | the app, to sideload with AltStore, SideStore or Sideloadly (or TrollStore); see [ios/ReadMe.txt](../ios/ReadMe.txt) |
| [zsnes-ppc32-linux.tar.gz](zsnes-ppc32-linux.tar.gz) | 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt`, `zsnes.png` and `install-launcher.sh` (a desktop and menu launcher with the icon) |

The [Release](../../actions/workflows/release.yml) workflow (Actions tab, Run workflow) publishes these, with [RELEASE_NOTES.md](RELEASE_NOTES.md), as a GitHub release.

Each build has one read me, for its own system: requirements, keys, video
modes, troubleshooting, credits and license. Default controls: arrow keys
for the d-pad, X = A, Z = B, S = X, A = Y, D/C = L/R, Return = Start, Right
Shift = Select. Sound plays at 44100 Hz, converted from the SNES's 32000 Hz by
ZSNES's own filter.

**Mac OS 9:** needs 40 MB of free memory; drop a ROM on the icon. Video menu:
Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to
Monitor. OpenGL modes need Apple's OpenGL and an accelerated card. Tested on
a Power Mac 8500 with a Buffalo G4/500 MHz upgrade and a Radeon 7000.

**Mac OS X:** a Carbon application, run natively (not in Classic). The same
Video menu and shortcuts as on Mac OS 9; full screen hides the menu bar and
the Dock. OpenGL modes through Mac OS X's OpenGL. Keep the "ZSNES PPC" folder
somewhere you can write to: settings and saves go next to the application.

**PlayStation 4:** native homebrew made with OpenOrbis (GoldHEN). 1080p
frames flipped on the vertical blank, 4:3; DualShock 4s of the logged-in users
are players 1 to 4; Options+touch pad or R3 opens the menu, L3 the frame
counter. Built on GitHub by the [PS4](../../actions/workflows/ps4.yml) workflow.

**PlayStation 3:** native homebrew on the Cell's PowerPC core (PSL1GHT). The
RSX scales the picture 4:3 to the PS3's resolution; DualShock 3 pads are
players 1 to 4 with no setting up, Start+Select opens the menu, L3 the frame
counter; a USB keyboard works too. The core was checked on 64-bit big-endian
PowerPC against x86 over the 164 test ROMs.

**iPhone:** an on-screen SNES controller beside a 4:3 picture, with MENU and FPS
buttons; a Bluetooth controller (Switch Pro by its letters, Xbox and PlayStation
by position) works as player 1 at once and hides the on-screen controls, and
Start+Select opens the menu. ROMs go in the app's folder in the Files app. Built
on GitHub's Mac runners by the [iOS](../../actions/workflows/ios.yml) workflow.

**PowerPC Linux:** run `./zsnes game.smc` from a terminal in X, or
`./install-launcher.sh` once for a launcher with the ZSNES icon. Ctrl-F
frame counter, Ctrl-Shift-F or Alt-Enter full screen. Plays straight to the
sound card, through aRts only when the card is busy. OpenGL modes with DRI
(the read me has the XF86Config that worked for a PCI Radeon 7000); full
screen keeps the desktop's resolution. Tested on Yellow Dog Linux 3.0.1 with
kernel 2.4.22.

SHA-256:

```
7aa4ae6e40b40407b3905056dcdea82fa6c0a142931d10abad7f34997a6997ee  zsnes-ppc-classic-macos.sit.hqx
a5922e4bd9322551ac6b900af7d7ff59285eee32a2c23c240c83d65b9aae974a  zsnes-ppc-macosx.sit.hqx
d4499dddd3e395dc3b698f1cefdc535f3b23989a03332d879f75e1d195c25a8f  zsnes-ps3.pkg
ba149271a4cf68a4497d9e98bc92950e353c544195384e0196b7f72bceed0aaa  zsnes-ps4.pkg
b3fea510b0a261052269e81d0051d873a519430e135a7429d6b904b0f9b9eddc  zsnes-ios.ipa
cd91bcb23bdba94f0b56865c95bc197ff49da2108d49d6fe82a42c4c56a894b0  zsnes-ppc32-linux.tar.gz
```

Built with `tools/build-macos9.sh`, `tools/build-ydl.sh`, `tools/build-ps3.sh` and
`tools/build-ios.sh`, and packaged with `tools/package-release.sh`.

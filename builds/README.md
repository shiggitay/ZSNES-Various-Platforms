# ZSNES PPC builds

Ready-to-run builds of this repository. No ROMs are included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-classic-macos.sit.hqx](zsnes-ppc-classic-macos.sit.hqx) | Mac OS 8.6 to 9.2.2, PowerPC | double-click it: StuffIt Expander (included with Mac OS 8 and 9) gives you a "ZSNES PPC" folder with the application and its read me |
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

**PowerPC Linux:** run `./zsnes game.smc` from a terminal in X, or
`./install-launcher.sh` once for a launcher with the ZSNES icon. Ctrl-F
frame counter, Ctrl-Shift-F or Alt-Enter full screen. Plays straight to the
sound card, through aRts only when the card is busy. OpenGL modes with DRI
(the read me has the XF86Config that worked for a PCI Radeon 7000); full
screen keeps the desktop's resolution. Tested on Yellow Dog Linux 3.0.1 with
kernel 2.4.22.

SHA-256:

```
52cc461aac3d5d9fbab3310693300b9a1d4795a4417ae0f56be62cc370f6d19d  zsnes-ppc-classic-macos.sit.hqx
a183b10186e286656cdcfadbace80b71ea7582ccae1b5335cb840b7192b191c5  zsnes-ppc32-linux.tar.gz
```

Built with `tools/build-macos9.sh` and `tools/build-ydl.sh`, and packaged
with `tools/package-release.sh`.

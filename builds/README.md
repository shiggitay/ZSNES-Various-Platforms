# ZSNES PPC builds

Ready-to-run builds of this repository. No ROMs are included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-classic-macos.sit.hqx](zsnes-ppc-classic-macos.sit.hqx) | Mac OS 8.6 to 9.2.2, PowerPC | double-click it: StuffIt Expander (included with Mac OS 8 and 9) gives you a "ZSNES PPC" folder with the application and its read me |
| [zsnes-ppc32-linux.tar.gz](zsnes-ppc32-linux.tar.gz) | Yellow Dog Linux 3 and other 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt` |

The [Release](../../actions/workflows/release.yml) workflow (Actions tab, Run workflow) publishes these, with [RELEASE_NOTES.md](RELEASE_NOTES.md), as a GitHub release.

Each build has one read me, for its own system: requirements, keys, video
modes, troubleshooting, credits and license.

**Mac OS 9:** needs 40 MB of free memory; drop a ROM on the icon. Video menu:
Show FPS (Command-F), Full Screen (Shift-Command-F or Command-Return), Sync to
Monitor. OpenGL modes need Apple's OpenGL and an accelerated card. Tested on
a Power Mac 8500 with a G4/500 upgrade and a Radeon 7000.

**Yellow Dog Linux:** run `./zsnes game.smc` from a terminal in X. Ctrl-F
frame counter, Ctrl-Shift-F or Alt-Enter full screen. Plays straight to the
sound card, through aRts only when the card is busy. OpenGL modes with DRI
(the read me has the XF86Config that worked for a PCI Radeon 7000); full
screen keeps the desktop's resolution. Tested on Yellow Dog Linux 3.0.1 with
kernel 2.4.22.

SHA-256:

```
778f9b128d1968c0de6e28f5f90b36c672d96c48a6761c3c66b37cca7087e5d2  zsnes-ppc-classic-macos.sit.hqx
48085df751cedff6c4cd86932ab63c4794ed7c0d8873ec9dd0cc07bdf4313b1e  zsnes-ppc32-linux.tar.gz
```

Built with `tools/build-macos9.sh` and `tools/build-ydl.sh`, and packaged
with `tools/package-release.sh`.

# ZSNES PPC builds

Ready-to-run builds of this repository. No ROMs are included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-macos9.sit.hqx](zsnes-ppc-macos9.sit.hqx) | Mac OS 8.6 to 9.2.2, PowerPC | double-click it: StuffIt Expander (included with Mac OS 8 and 9) gives you a "ZSNES PPC" folder with the application and its read me |
| [zsnes-ppc32-ydl.tar.gz](zsnes-ppc32-ydl.tar.gz) | Yellow Dog Linux 3 and other 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt` |

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
3e8b92c24aa8fbcfee53489e587492f6a90275b758150d873cdde9450d6a136e  zsnes-ppc-macos9.sit.hqx
c5c2c10176e6a01c8264636b7068be4f81e8d3a5b28215b2a79da22562da5773  zsnes-ppc32-ydl.tar.gz
```

Built from b420eac with `tools/build-macos9.sh` and `tools/build-ydl.sh`.

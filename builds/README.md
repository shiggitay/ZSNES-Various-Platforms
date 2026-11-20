# ZSNES PPC builds

Ready-to-run builds of this repository, named by build number. No ROMs are
included.

| Package | For | Contents |
|---|---|---|
| [zsnes-ppc-macos9-build10.tar.gz](zsnes-ppc-macos9-build10.tar.gz) | Mac OS 8.6 to 9.2.2, PowerPC | `ZSNES.bin` (MacBinary, for StuffIt Expander), `ZSNES.dsk` (HFS disk image), `ReadMe.txt` |
| [zsnes-ppc-ydl-build10.tar.gz](zsnes-ppc-ydl-build10.tar.gz) | Yellow Dog Linux 3 and other 32-bit PowerPC Linux (glibc 2.3, SDL 1.2, Linux 2.4 or 2.6) | `zsnes`, `README.txt` |

The [Release](../../actions/workflows/release.yml) workflow (Actions tab, Run workflow) publishes these, with [RELEASE_NOTES.md](RELEASE_NOTES.md), as a GitHub release.

Each package has a read me with requirements, keys and video modes.

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
b5ae7693b82f475cdb1ba8390f5dc83e5445fcde20335168dfc7745fb441f844  zsnes-ppc-macos9-build10.tar.gz
5830887c9ed63559d5d0de00f33388a26114f7189a0ced6834a458290269d4b2  zsnes-ppc-ydl-build10.tar.gz
```

Built from b420eac with `tools/build-macos9.sh` and `tools/build-ydl.sh`.

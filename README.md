# ZSNES PPC

<img align="right" width="128" height="128" alt="logo" src="img/128x128x32.png">

ZSNES PPC is ZSNES for PowerPC Macs: a fork of
[xyproto/zsnes](https://github.com/xyproto/zsnes) (ZSNES 2, the C port) that
runs on

* **classic Mac OS 8.6 to 9.2.2**, as a native PowerPC application, with
  OpenGL video modes through Apple's OpenGL, full screen, and a Video menu
  (frame counter on Command-F, full screen on Shift-Command-F or
  Command-Return);
* **Mac OS X on PowerPC**, as a native Carbon application with the same
  features, the menu bar and the Dock hidden in full screen; and
* **Yellow Dog Linux 3** and other 32-bit PowerPC Linux of its age (Linux 2.4
  or 2.6, glibc 2.3, SDL 1.2, and theoretically newer 32-bit PowerPC Linux distros as well), with the same OpenGL modes through SDL 1.2 and
  the same shortcuts on Control.

The emulator core is byte-order independent: on the test suite and 164 test
ROMs a PowerPC build produces the same machine state, picture and sound as an
x86 one, frame for frame. Sprite, Mode 7 and background drawing were made
markedly faster for the slower machines. Ready-to-run builds for all three are in
[builds/](builds/); how to make them is under
[Classic Mac OS](#classic-mac-os-mac-os-86-to-922-powerpc) and
[32-bit PowerPC](#32-bit-powerpc-yellow-dog-linux-g3g4g5-ps3) below. The rest
of this file is the upstream README.

---

# ZSNES 2

[![CI](https://github.com/xyproto/zsnes/actions/workflows/build.yml/badge.svg)](https://github.com/xyproto/zsnes/actions/workflows/build.yml)

The last release of [ZSNES](http://zsnes.sourceforge.net/) was over 18 years ago (2007-10-31). The development of ZSNES started in 1997. It requires heavy patching and tweaking of compilation flags to build and run on a modern Linux distro. Most of it was written in 32-bit x86 Assembly, with MMX instructions.

This is a fork that aims to solve this.

Goals and non-goals:

* Compiling ZSNES so that it works on 64-bit x86 is a goal (that has been reached).
* Porting the Assembly to C11 is a goal (that has been reached).
* Supporting modern Linux distros is a goal (that has been reached).
* Supporting FreeBSD and macOS is a goal (if possible).
* Supporting multiple CPUs and platforms is a goal (that has been reached).
* Supporting DOS is not a goal.
* Supporting Windows is not a goal, but a "nice to have".
* Improving the netplay feature is a long term goal.
* To be like an LTS release of ZSNES is a goal.

Pull requests that fixes inaccuracies with the emulation are welcome, as well as pull requests for regular bugs.

### Requirements

* `python3`, `sdl3`, `pipewire` (or `libao`), and a C compiler like `gcc` or `clang`.

Tested on Arch Linux, Fedora and Debian 12 on x86_64, and on macOS on aarch64.

### Build

    make

These specific targets are available (all but `linux_x86_64` may be a bit experimental):

```sh
make win_i686
make win_x86_64
make win_aarch64
make linux_i686
make linux_x86_64
make linux_aarch64
make linux_riscv64
make macos_x86_64
make macos_aarch64
make freebsd_x86_64
make freebsd_aarch64
```

* Cross targets require a suitable target compiler and target libraries.
* Run `make help` for a complete target list.

### Run a ROM

Point ZSNES at a ROM after building it:

```sh
# Linux, macOS, or BSD
./zsnes ~/roms/snes/example.sfc

# Windows from MSYS2 or Git Bash
./zsnes.exe ~/roms/snes/example.sfc
```

Replace `example.sfc` with the path to your own ROM; ROM files are not included.

### Debug build

Build `zsnes` with debug symbols and debugger-friendly optimization:

    make debug

For runnin `zsnes` with the same ROM, under GDB:

```sh
gdb --args ./zsnes ~/roms/snes/example.sfc
# Use ./zsnes.exe instead on Windows.
```

At the GDB prompt, enter `run`. If ZSNES crashes, enter `bt full` to print a
detailed backtrace. With LLDB, use:

```sh
lldb -- ./zsnes ~/roms/snes/example.sfc
```

Then enter `run`, followed by `bt all` after a crash.

### Install

    make install

Installing a desktop shortcut is possible. A `zsnes.desktop` file is included in `linux/zsnes.desktop`. It could also be generated with [gendesk](https://github.com/xyproto/gendesk). If you have a suitable icon named `zsnes.png` then they can both be installed with:

    install -Dm644 zsnes.png /usr/share/pixmaps/zsnes.png
    install -Dm644 zsnes.desktop /usr/share/applications/zsnes.desktop

For other platforms than Linux, different flags may apply. ZSNES 2 is primarily one executable, but for UNIX-inspired operating systems, the man page (`man/zsnes.1`) can be installed as ie. `/usr/share/man/man1/zsnes.1` (this is handled by `make install`, though).

### macOS

Install the dependencies with [Homebrew](https://brew.sh/), then build:

```sh
brew install libao libpng pkgconf python3 sdl3
git clone https://github.com/xyproto/zsnes
cd zsnes
make
```

### Debian and Ubuntu

Try installing ZSNES from [this flatpak](https://flathub.org/apps/io.github.xyproto.zsnes), or see if the following commands works for you, on **x86_64**. Please create a PR if they don't:

For a 32-bit x86 build while on x86_64:

```sh
dpkg --add-architecture i386
apt update
apt install -y git make pkg-config python3 zlib1g-dev build-essential
git clone https://github.com/xyproto/zsnes
cd zsnes
make
apt install -y gcc-multilib libglvnd-dev libglvnd-dev:i386 libpng-dev libpng-dev:i386 libsdl3-dev libsdl3-dev:i386 zlib1g-dev zlib1g-dev:i386
make
```

### Cross-Compiling for Windows on Debian/Ubuntu

Try the following commands in ie. Ubuntu 24 running under WSL:

```sh
apt update
apt install -y git make mingw-w64 libz-mingw-w64-dev python3 pkg-config build-essential
git clone https://github.com/xyproto/zsnes
cd zsnes
make ARCH=win WITH_PNG= CC_TARGET=i686-w64-mingw32-gcc CC=i686-w64-mingw32-gcc WINDRES=i686-w64-mingw32-windres
```

### Compiling for Windows with MSYS2

Use an MSYS2 MinGW shell matching the desired target architecture.

Run the following commands. Please create a PR if you have issues.

Tested on MSYS2 x86_64 version 20260322.

```sh
pacman -Syu
pacman -Sy git make pkg-config python3 mingw-w64-x86_64-gcc mingw-w64-x86_64-libpng mingw-w64-x86_64-zlib
make
```

Use `make win_i686` or `make win_x86_64` when selecting an architecture explicitly.

### Compiling for Windows on ARM with MSYS2

Use the MSYS2 CLANGARM64 shell. This builds a native ARM64 `zsnes.exe`.

```sh
pacman -Syu
pacman -S --needed git make mingw-w64-clang-aarch64-clang mingw-w64-clang-aarch64-python mingw-w64-clang-aarch64-pkgconf mingw-w64-clang-aarch64-libpng mingw-w64-clang-aarch64-zlib
make
```

Tested on Windows 11 ARM64 with MSYS2 CLANGARM64.

### 32-bit PowerPC (Yellow Dog Linux, G3/G4/G5, PS3)

`make linux_ppc` builds a 32-bit big-endian PowerPC binary, natively on a PowerPC
host or with a `powerpc-linux-gnu` cross compiler. A `ppc64` kernel with a 32-bit
userland, as on the PS3 and the G5, counts as PowerPC too.

The emulator core, both renderers, sound, MSU-1 and the coprocessors are
byte-order independent: on the test suite in `test/` and on a set of 164 test
ROMs, a PowerPC build produces the same machine state, the same picture and the
same audio samples as an x86 build, frame for frame.

Systems of Yellow Dog Linux's era have SDL 1.2 rather than SDL3. Build against
it with `WITH_SDL12=yes`, which puts `unix/sdl12compat.c` - the part of SDL3 the
frontend uses, written on top of SDL 1.2 - in place of SDL3:

```sh
make linux_ppc WITH_SDL12=yes
```

With SDL 1.2 the video is software only (no OpenGL or accelerated renderer,
and scaled modes are nearest-neighbour), every controller is a plain joystick
(no gamepad mapping, no rumble), and audio goes through SDL 1.2's own drivers.
Key bindings are stored with SDL3's numbers either way, so a configuration file
moves between the two builds.

Cross-compiling from Debian or Ubuntu, with SDL 1.2 and libpng built for the
target and their `sdl-config` and `.pc` files pointed at:

```sh
apt install -y gcc-powerpc-linux-gnu
make linux_ppc WITH_SDL12=yes SDL_CONFIG=/path/to/ppc/bin/sdl-config \
  LINUX_PPC_PKG_CONFIG=/path/to/powerpc-linux-gnu-pkg-config
```

For Yellow Dog Linux itself, `tools/build-ydl.sh` does the whole cross build on
Debian or Ubuntu: it fetches Debian 4.0's PowerPC glibc 2.3.6 and SDL 1.2.11 from
archive.debian.org (checking them against the archive's SHA1 sums), builds
against them, and leaves a `zsnes` that needs nothing newer than `GLIBC_2.3`
and `libSDL-1.2.so.0` - which Yellow Dog Linux 3.0 and later all have. That build
saves screenshots as BMP rather than PNG.

```sh
apt install -y gcc-powerpc-linux-gnu curl python3 make pkg-config
tools/build-ydl.sh
```

The tree is C11: building on the PowerPC machine itself needs GCC 6 or newer.
To tune for a particular core, pass `PPC_CFLAGS`, for example
`PPC_CFLAGS=-mcpu=7450` (G4), `-mcpu=970` (G5) or `-mcpu=cell` (PS3); the
default runs on any 32-bit PowerPC, the G3 included.

Save states hold the emulator's memory in the host's byte order, so a state
saved on PowerPC loads on PowerPC and one saved on x86 loads on x86. Battery
saves (`.srm`), movies, cheats, configuration files and netplay are the same on
both.

### Classic Mac OS (Mac OS 8.6 to 9.2.2, PowerPC)

`tools/build-macos9.sh` builds ZSNES as a classic Mac OS application with the
[Retro68](https://github.com/autc04/Retro68) cross toolchain:

```sh
git clone https://github.com/autc04/Retro68
cd Retro68 && git submodule update --init && cd ..
mkdir retro68-build && cd retro68-build
../Retro68/build-toolchain.bash --no-68k --no-carbon
cd ../zsnes
RETRO68=../retro68-build/toolchain tools/build-macos9.sh
```

It leaves `build-macos9/ZSNES.bin` (MacBinary, for StuffIt Expander) and
`build-macos9/ZSNES.dsk` (an HFS disk image with the application and a read
me). The frontend is the Unix one: `macos9/toolbox.c` is the part of SDL3 it
uses, written on the Toolbox (a window and a 16-bit GWorld, GetKeys, the Sound
Manager), and `macos9/posix.c` gives it Unix paths over the File Manager, `/`
being the list of mounted disks. It links against InterfaceLib and
AppearanceLib, and weakly against OpenGLLibrary (for the OpenGL video modes)
and ControlStripLib (hidden in full screen), so it starts without either. There are no joysticks or netplay yet.
Pass `MACOS9_CFLAGS=-mcpu=7400` (or `750`) to tune for a G4 (or G3).

`CARBON=1 tools/build-macos9.sh` builds the same frontend as a Carbon
application, into `build-macos9-carbon/`, which Mac OS X runs natively. It
needs Retro68 built with its Carbon libraries (leave out `--no-carbon`). The
Carbon build links against CarbonLib alone, plus OpenGLLibrary weakly; it
reaches window ports through Carbon's accessors, flushes each frame to Mac OS
X's window buffer, and hides the menu bar and the Dock in full screen through
`SetSystemUIMode`, found at run time in the Carbon framework. It also carries
a Mac OS X icon family (`macos9/icns.r`, from `tools/mkicns.py`).
`tools/package-release.sh` makes the three packages in `builds/`.

### Pull requests

* Pull requests are welcome.
* Being able to build and run ZSNES on Linux is a priority.
* Please have `clang-format` installed and run `make fmt` before submitting a pull request.

### Windows users

* Compiled executables for ZSNES 1.51 for Windows are available at https://sourceforge.net/projects/zsnes/files/zsnes/ZSNES%20v1.51/
* Pull requests for supporting Windows are welcome, but it's not a goal for this project.

### General info

Thanks to Christoph Mallon for the commits that this fork is based on.

### List of contributors

These are the contributors listed in the text files that are included with the 1.51 release of ZSNES, and were mentioned on the [zsnes.com](https://zsnes.com/old) webpage:

* `zsKnight`
* `_Demo_`
* `pagefault`
* `Nach`
* `grinvader`
* `Deathlike2`
* `Jonas Quinn`
* `blargg`
* `Pharos`
* `teuf`
* `relnev`
* `prometheus`
* `theoddone33`
* `EvilTypeGuy`
* `stainless`
* `aaronl`
* `MKendora`
* `kode54`
* `zinx`
* `amit`
* `hpsolo`
* `Kreed`
* `Neviksti`
* `ipher`
* `relnev`
* `StatMat`
* `MKendora`
* `hpsolo`
* `aaronl`
* `Diablo-D3`
* `Wilbern Cobb`
* `Thorsten "mirabile" Glaser`
* `Mitchell "The Khan Artist/Noxious Ninja" Mebane`

### Previous documentation

* Some of the information in the old text files in `docs/readme.txt/*.txt` has not yet been ported over to the `man/zsnes.1` man page or to a Markdown document.
* The plan is to do this. Pull requests are welcome.

Additional old documentation:

* [doc/html](doc/html)

### General info

* License: GPL2
* Version: 2.3.9
* Fork author: Alexander F. Rødseth &lt;xyproto@archlinux.org&gt;

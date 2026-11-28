# The downloads: Windows, Linux, Raspberry Pi and macOS

The game's code may never be distributed (it is made from the disc), so each download carries everything but that code, and the player's own computer recompiles their `default.xbe` and compiles the result. One launcher does it on every system: `tools/player-kit/launcher`, Toy Commander's (ToyCommander--QilinRecomp's, the master copy), copied unchanged, as GBTGhoulies--QilinRecomp has it. It names no game; what it knows about this one is the download's `kit/game.txt`, written by `tools/player-kit/games.sh`, and what the player's folder is, `qilin identify` says. `launcher/ADOPTING.md` is the contract: the `game.txt` keys, the identify contract, what a project provides, and `LAUNCHER_VERSION` (this copy: 3), which says how far behind the master a copy is. A fix to the launcher belongs in the master and is copied out again.

| System | Download | Compiler | Notes |
|---|---|---|---|
| Windows 10 (1803) or 11, x64 | `BarbieWildHorseRescue--QilinRecomp-X.Y.Z-windows-x64.zip`, `Play Wild Horse Rescue.exe` | LLVM-MinGW 20261104, bundled (`trim-toolchain.sh`) | unsigned: SmartScreen's "More info, Run anyway" once |
| Linux x86-64 and ARM64 | `-linux-x86_64.tar.gz`, `-linux-arm64.tar.gz`, `Play Wild Horse Rescue` | zig 0.17.1, bundled: its own libc++, the system's glibc (2.35 or later) | menu entry from the launcher; needs a Vulkan 1.3 driver |
| macOS 14.5+ (Xcode 16's tools need it), Apple silicon and Intel | `-macos.dmg`, `Wild Horse Rescue.app` (universal) | Apple's clang from the Command Line Tools, which the launcher offers to install | MoltenVK v1.4.2 inside the app; signed ad hoc: "Open Anyway" in Privacy & Security once |

The files are named `Wild Horse Rescue` (as the clone's `Play Wild Horse Rescue.bat` is) because the game's whole name has a colon, which Windows doesn't allow in a file name; the READMEs use the whole name.

## What the player gives it

An extracted disc: the folder holding `default.xbe` and the game's `Packages` and `Video` folders (`input=folder` in `game.txt`), made with extract-xiso or xdvdfs.

A folder rather than the disc image because the runtime reads the game's files from that folder while it runs (`qilin_run <folder>`; `kernel_io.cpp` maps the disc onto it). Taking the `.iso` would mean a first step extracting all 588 MB of it into `work/` (a second copy on the player's disk, read through again at every rebuild), and a player who already has an extracted folder couldn't use it. `qilin identify` makes the image case plain instead: a folder holding an Xbox disc image, or the image itself, is told to be extracted first, and a folder one level above the extracted disc is told which folder to choose.

The launcher runs `qilin identify <folder> --json --build "Barbie Horse Adventures: Wild Horse Rescue (USA)"` (`recomp/src/identify.cpp`) when the folder is chosen and at every start:

| Exit | When | The message says |
|---|---|---|
| 0 | the certificate's title ID is 5655002A (VU-042) and the XBE's timestamp 1064594583 (26 September 2003): the USA release | "This is Barbie Horse Adventures: Wild Horse Rescue (USA), VU-042 (built 26 September 2003): the right disc." |
| 3 | another title ID | the game the certificate names, its regions and ID, and what this download is for |
| 3 | the same title ID, another timestamp | another release of the game: its regions and build date, and the one needed |
| 2 | no such folder; a disc image, chosen itself or lying in the folder; another file; no `default.xbe` (any case), with the folder inside that has one if there is; an XBE that doesn't parse; no `Packages` or `Video` folder (any case) | what's wrong and how to extract the disc |

`title`, `id` (the certificate's title ID, as "VU-042") and `version` (the certificate's version: 5 on the USA disc) describe what was chosen. Nothing is written; only `default.xbe` is read, or a candidate image's file system.

## The build on the player's computer

`game.txt`'s one step is `{qilin} lift {input} --out {gen}` (`qilin` finds the folder's `default.xbe` whatever its case): 6,772 functions into 34 files (55 MB; the largest 4.3 MB) in about five seconds. The launcher compiles every `.cpp` there with `build-flags.txt`'s flags (`-O2 -std=c++23 -ffp-contract=off -fno-fast-math`, the same floating-point rules as `qilin_run`, plus this processor's `-march=native` or `-mcpu=native`) and links them with `kit/lib/libwildhorse_kit.a`: the runtime's start-up and dispatch (`wildhorse_kit_main`) and every library they need, SDL3 and volk included. The largest file took about 40 s and 1,160 MB with zig's Clang on a desktop PC, so `compile_memory_mb=1300`: a 2 GB Raspberry Pi compiles one file at a time. The game is started as `work/WildHorseRescue[.exe] <folder>`, in the download's folder; its output goes to `work/run.log` and `work/run-errors.log`. Saves and settings stay in the runtime's own folder, `qilin/wildhorserescue` in the system's per-user data folder, which is the launcher's too (the chosen folder in `launcher-disc.txt`, the object cache in `build-cache`).

`old_files` and `old_input_file` (launcher 2) name what the Windows downloads before the launcher (0.13 to 0.16) left behind: `Play Wild Horse Rescue.bat`, the game beside it (`WildHorseRescue.exe`, `.previous`, `.new`) and its logs, `kit/play.ps1`, Ninja and its files, `work/built-with.txt`, and their remembered disc folder (`work/disc-folder.txt`), so a new download unzipped over an old one tidies them away and keeps the player's choice.

All the tools and the runtime take UTF-8 paths, reading Windows' command line as UTF-16 (`recomp/runtime/command_line.hpp`), so a disc folder or user name outside the system's code page works.

## How each is built and checked (CI)

- **Windows** (`.github/workflows/player-kit.yml`, `tools/player-kit/package.sh`): cross-compiled on Ubuntu with llvm-mingw (`mingw-cross.cmake`); the kit's archive merged by an MRI script (`merge.cmake`) and linked with stand-ins for the lifted code (`wildhorse_kit_link_check`); the game's icon (`game_icon.rc`, the launcher's) is `kit/lib/icon.o`; a 16 MB main stack. The workflow unzips the result and checks its layout against `kit/files.txt`.
- **Linux** (`player-kit-linux.yml`, `package-linux.sh`): built on Ubuntu 22.04 (x86-64 and ARM64 runners) with the zig the download bundles (`zig cc`/`zig c++` with `-mcpu=baseline`), so the archive's libc++ is the one the player's link uses and glibc 2.35 is the floor. SDL's own Wayland protocol headers come before the system's (`-iquote`), as zig searches `/usr/include` first. The launcher's tests and `qilin_tests` run with the bundled zig; `smoke-linux.sh` unpacks the tarball and checks the layout, compiles and runs a C++23 program with `build-flags.txt`, links the kit with an empty function table and runs it (it must stop with its usage), runs `qilin identify` on stand-ins, and checks every program's glibc symbols and libraries.
- **macOS** (`player-kit-macos.yml`, `package-macos.sh`, `macos.cmake`): universal (`arm64;x86_64`), the archive merged by Apple's `libtool`, `build-flags.txt` with each processor's own flags (`native.arm64`, `native.x86_64`, `ldflags.<arch>`). The app holds the launcher, MoltenVK's own `libMoltenVK.dylib` (pinned by SHA-256; the launcher starts the game with `DYLD_FALLBACK_LIBRARY_PATH` and `SDL_VULKAN_LIBRARY` pointing at it), the kit and the licences, and is signed ad hoc. `check-macos.sh` runs on Apple silicon and Intel: the layout, `lipo`, `codesign --verify --deep --strict`, `files.txt`, `qilin identify`, and a stand-in game compiled with Apple's clang and the kit's flags, started (it must stop with its usage).
- On a `vX.Y.Z` tag each workflow attaches its files to that release, creating it if none of the others has yet.

## Raspberry Pi

The Linux ARM64 download, built on the Pi itself. Vulkan 1.3 for the Pi 4 and 5 came with Mesa 24.3 (Raspberry Pi OS Trixie); the launcher checks the driver before building and says to update an older system. It needs the 64-bit Raspberry Pi OS (the README says so first, with `getconf LONG_BIT`). The first build compiles the whole game on the Pi: measure it there.

## Not in this pass

The iPad ("Play on iPad" from the Mac app, which Toy Commander has): the launcher shows it only where the app carries `Contents/Resources/ios-kit`, which this download doesn't. It would need the runtime ported to iPadOS first (its own stack for the game, MoltenVK linked statically, the disc folder in Documents).

## What only a player's run can confirm

The launcher's window, folder chooser and drag-and-drop on each desktop; a whole first build and the game starting from it on each system (CI builds and links only stand-ins); the old Windows download's leftovers tidied away and its disc folder taken over; the menu entry and shortcut; an update from one release to the next; Gatekeeper's "Open Anyway"; and the time and memory of a first build on a Raspberry Pi 4 and 5.

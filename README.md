# BarbieWildHorseRescue--QilinRecomp

A static recompilation of **Barbie Horse Adventures: Wild Horse Rescue** (Blitz Games / Vivendi Universal Games, original Xbox, 2003) into portable C++23 with a Vulkan 1.3 renderer, for 64-bit Windows, Linux and macOS on x86-64 or ARM64. Any GPU with a Vulkan 1.3 driver is in scope, down to the Raspberry Pi 4 (Mesa 24.3+). Macs run it through MoltenVK (macOS 10.15 or later).

> **No game data is included, and none ever will be.** You need your own copy of the game: the USA release. The tools read your disc image, or the files extracted from it, from a path you give them.

## Status

**It runs the game's opening scenes, not yet the whole game.** With scripted input it runs without a stop from power-on through the intro movie and menus to the stable yard, the dressing room and the stables, and plays all four mini-games to their results. All nine trails load and can be ridden once unlocked in memory. Nothing has been played through.

| Part | State |
|---|---|
| Disc image reader (full-disc dump or game partition) | ✅ tested on synthetic images, malformed ones included; not yet on a real image |
| XBE loader, the supported build recognised by SHA-256 | ✅ reads the game |
| x86 decoder (Zydis, vendored) | ✅ agrees with LLVM on all 500,668 reachable instructions |
| Function discovery, control flow and byte coverage | ✅ 0 unknown bytes in lifted regions; the 73 unresolved indirect jumps are all run-time targets ([`docs/code-analysis.md`](docs/code-analysis.md)) |
| Lifter, x86 → C++ | ✅ all 6,772 functions lift and compile |
| Differential test against the original x86 | ✅ 6,772 functions, 40,632 trials, 0 mismatches |
| Runtime: kernel, scheduler, file system; `qilin_run` | ✅ the game starts at its lifted entry point and runs |
| Renderer (Vulkan 1.3), audio engine, window, controllers, sound output (SDL3) | ✅ driven by the game |
| Settings menu and settings file | ✅ every entry acts on this game |
| Controller library | ✅ native |
| DirectSound | 🔨 native over the mixer, with 3D positioning |
| Direct3D | 🔨 native over the renderer, with fixed-function lighting ([`docs/graphics.md`](docs/graphics.md)) |
| XMV movie player | ✅ lifted whole, through the video overlay |
| Enhancements | ✅ widescreen (Hor+, up to 4:1), HUD placement, 1× to 4× resolution, anisotropic filtering, frame rate, five text languages |
| Downloads for Windows, Linux (x86-64, ARM64) and macOS, with the shared launcher | ✅ built and checked by CI on every push; not yet run by a player |

Latest validation record: [`docs/validation-0.16.md`](docs/validation-0.16.md).

## Playing

### Downloads

Each release on the [Releases page](https://github.com/Qilin1066/BarbieWildHorseRescue--QilinRecomp/releases) has a download for each system:

| System | Download | Then |
|---|---|---|
| Windows 10 or 11, 64-bit | `BarbieWildHorseRescue--QilinRecomp-<version>-windows-x64.zip` | unzip it, double-click **Play Wild Horse Rescue.exe** |
| Linux x86-64 (glibc 2.35 or later) | `BarbieWildHorseRescue--QilinRecomp-<version>-linux-x86_64.tar.gz` | unpack it, double-click **Play Wild Horse Rescue** |
| Linux ARM64: Raspberry Pi 4 or 5 (64-bit Raspberry Pi OS) | `BarbieWildHorseRescue--QilinRecomp-<version>-linux-arm64.tar.gz` | the same |
| macOS 14.5 or later, Apple silicon or Intel | `BarbieWildHorseRescue--QilinRecomp-<version>-macos.dmg` | drag the app onto Applications and open it; the first time, Privacy & Security's **Open Anyway** |

Then choose the folder you extracted your disc into (the one holding `default.xbe`, `Packages` and `Video`), or drag it onto the window: it says straight away if it's another game, another release, or a disc image still to be extracted (with extract-xiso or xdvdfs). The first start builds the game from it on your computer, in a few minutes (longer on a Raspberry Pi); after that, **Play** starts it. Each download's `README.txt` has the details, the controls and where saves go. Windows and Linux bring their own compiler (LLVM-MinGW, zig); a Mac uses Apple's command line tools, which the app offers to install. A graphics driver with Vulkan 1.3 is needed everywhere (on a Pi, Raspberry Pi OS Trixie or later). The game's code is built from your copy and never distributed. The launcher is the one all the QilinRecomp projects share (`tools/player-kit/launcher`, from ToyCommander--QilinRecomp); [`docs/platforms.md`](docs/platforms.md) describes the downloads.

### From the source on Windows

With Visual Studio 2022, **`Play Wild Horse Rescue.bat`** in a clone builds and starts the game; **`Update Wild Horse Rescue.bat`** pulls the latest code first. Both run `tools/build-wildhorse.ps1`.

### Linux and macOS

From a clone, with CMake 3.25 and GCC 14 or Clang 19 (Xcode 16 on macOS; on Linux, the SDL packages under [Building](#building)):

```sh
tools/build-wildhorse.sh <disc folder> --run
```

This builds the recompiler, lifts your `default.xbe` into `build-game/gen`, builds the game and starts it. After a `git pull`, run it again (with no folder it reuses the last); only what changed is rebuilt. The game is `build-game/qilin_run "<disc folder>"`.

## Building

The whole project is **C++23**. You need CMake 3.25 or later and one of:

| Compiler | Minimum | Notes |
|---|---|---|
| GCC | 14 | Debian 13 / Raspberry Pi OS (Trixie) and Ubuntu 24.04 (`g++-14`) have it |
| Clang | 19 | Older Clang can't use libstdc++'s `std::expected` |
| Apple Clang | Xcode 16 | |
| MSVC | Visual Studio 2022 17.10 | |

CMake stops with a clear message if the compiler is older.

The one other dependency is **SDL3**. CMake uses an installed SDL3 (3.2 or later), or else downloads SDL 3.4.20, checks its SHA-256 and builds it static. On Linux, SDL needs these development headers for a window and sound (Debian, Ubuntu, Raspberry Pi OS):

```sh
sudo apt install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev libegl-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev
```

Windows and macOS need nothing more.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CI builds and tests with warnings as errors (`-DQILIN_WERROR=ON`) on Linux x64 (GCC 14, Clang 19), Linux ARM64 (GCC 14), macOS ARM64 and x64, and Windows x64 (MSVC).

## Using it

`<game>` is your copy: a disc image, an extracted disc's folder, or its `default.xbe`.

```sh
build/qilin info     <game> [--json out.json]   # header, sections, XDK libraries, kernel imports, SHA-256
build/qilin imports  <game>                     # the kernel functions it imports, by name
build/qilin disc     <image>                    # the files on a disc image
build/qilin extract  <image> --out DIR [--only PATH]   # outside the repo, or an ignored folder
build/qilin cfg      <game> [--json cfg.json]   # function discovery and control flow
build/qilin align    <game>                     # how directly-called functions are aligned
build/qilin suspects <game>                     # kept functions with problems, for review
build/qilin coverage <game> [--list]            # every byte: code, table, padding or unknown (exit 3 if lifted code has unknown bytes)
build/qilin insns    <game> [--linear] -o listing.txt
build/qilin lift     <game> --out gen/          # lift to C++ (gen/ is gitignored: never commit it)
build/qilin_run      <extracted disc> [--data-dir DIR] [--ram-mb N]   # run it (built with -DQILIN_GEN_DIR=gen)
```

Another build is refused unless `--any-build` is given. `qilin_run` with no arguments lists its other options, including `--headless`, `--press`, `--dump-frames` and `--virtual-time`, which makes a scripted run repeatable frame for frame ([`docs/runtime.md`](docs/runtime.md)).

`build/qilin_menu_preview` shows the settings menu (F1 or Esc) over a test card, with no game needed.

Code that compiles lifted C++ must link the `qilin_runtime` target (or pass `-ffp-contract=off`): SSE results are bit-exact only without fused multiply-add. `-DQILIN_CHECKED_MEMORY=ON` bounds-checks every guest memory access.

### Checks

- **`ctest`**: unit tests on a synthetic XBE and disc image, and three hand-assembled programs lifted, built and run. No game files needed.
- **Decoder check** (`tools/check/verify_decoder.py`): Zydis against LLVM on the game's `qilin insns` listings; every reachable instruction agrees.
- **x87 checks** (`tools/check/x87/`): x87 arithmetic against exact results and mpmath. No game needed.
- **Differential test** (`tools/difftest/`): every lifted function against the original x86 under Unicorn on random states, registers and written memory compared bit for bit ([`docs/recompiler-core.md`](docs/recompiler-core.md)).

### Analysing your copy

On Windows, with Visual Studio 2022 17.10 or later ("Desktop development with C++") and Git, double-click **`Analyse game.bat`**. It builds `qilin`, asks for your disc image or `default.xbe`, and writes the file list, executable summary and SHA-256, kernel imports, functions and byte accounting to `analysis-out\`, which is never committed.

## Layout

| Path | What |
|---|---|
| `recomp/src/` | Recompiler core: `xiso` (disc images), `xbe` loader, `sha256`, `decoder` (Zydis wrapper), `cfg` and `vrange` (function discovery, control flow, jump-table sizes), `lifter` (x86 → C++), `game.hpp` (the supported build's facts), `main` (the CLI) |
| `recomp/runtime/` | Runtime headers the generated C++ includes (guest memory, flag and float helpers, x87 maths) |
| `recomp/runtime/gfx/` | The renderer: Vulkan 1.3, loaded at run time |
| `recomp/runtime/audio/` | The audio engine: voices (PCM, Xbox ADPCM) and the 48 kHz mixer |
| `recomp/runtime/platform/` | The window, the controllers and sound output, through SDL3; the settings file |
| `recomp/runtime/ui/` | The settings menu, and `qilin_menu_preview` |
| `recomp/runtime/host/` | The host side: `qilin_run`, the replacement registry, guest RAM, the kernel, the scheduler, the file system, Direct3D, DirectSound and the controller library |
| `recomp/tests/` | Unit tests and the hand-assembled test programs; none needs game files |
| `tools/build-wildhorse.sh`, `tools/build-wildhorse.ps1` | Build the game from a clone (Linux and macOS; Windows) |
| `tools/player-kit/` | The downloads (Windows, Linux, macOS): packaging, their checks and READMEs, and the launcher (`launcher/`, shared with the other QilinRecomp projects) |
| `tools/check/` | The LLVM decoder cross-check and the x87 checks |
| `tools/difftest/` | The differential test |
| `tools/gen/` | Generators for committed tables and shaders |
| `tools/analyse-game.ps1` | What `Analyse game.bat` runs |
| `third_party/` | Vendored dependencies: Zydis and Zycore, Dear ImGui, FSR 1, SMAA and volk (MIT), the Vulkan headers (Apache-2.0 or MIT), nxdk's kernel export list (CC0), XbSymbolDatabase's licence (MIT; the XDK function names). See `third_party/README.md` |
| `docs/` | The roadmap and design targets, the recompiler core, the runtime, graphics, the executable and code analysis, naming, prior art, validation records |

## Versions and changes

Releases are numbered `vX.Y.Z`, and every release is recorded in [`CHANGELOG.md`](CHANGELOG.md). The version lives in one place, `project(... VERSION ...)` in `CMakeLists.txt`, and `qilin --version` prints it. A minor bump (0.**x**) marks a milestone and a patch bump (0.x.**y**) a fix. 1.0.0 will be the first build that plays the whole game.

## Licence

This project's own code and documentation are under the [GNU General Public License, version 3](LICENSE) (GPL-3.0): you may use, share, change and sell it, but anything you distribute that is built from it must come with its full source under the same licence. Vendored code in `third_party/` keeps its own licences (`third_party/README.md`); SDL3 keeps its zlib licence.

The game itself belongs to its owners (Mattel, Vivendi Universal Games, Blitz Games) and is not covered by anything here: you need your own copy, and nothing built from it may be shared.

## Design targets

- **Efficiency and reliability come first.** Cheap to run on low-end hardware; fail loudly rather than misbehave quietly.
- **Static recompilation, x86 → C++, compiled ahead of time.** No interpreter and no JIT. Hand-written C++ can override any lifted function.
- **Vulkan 1.3 only.** Direct3D 8 is translated at API level, using 1.3 core plus what the Pi 4's driver offers, so one render path runs everywhere.
- **The kernel and the hardware-facing XDK libraries are replaced natively.** The C runtime and other pure-computation libraries are lifted with the game, for exact behaviour.
- **Ground truth is xemu** running the original game.

See [`docs/`](docs/) for the roadmap and the analysis notes.

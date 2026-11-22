# BarbieWildHorseRescue--QilinRecomp

A static recompilation of **Barbie Horse Adventures: Wild Horse Rescue** (Blitz Games / Vivendi Universal Games, original Xbox, 2003, the USA release) into portable C++23 with a Vulkan 1.3 renderer, aimed at 64-bit Windows, Linux and macOS, on x86-64 or ARM64. Anything with a Vulkan 1.3 driver is in scope, including the Raspberry Pi 4 (Mesa 24.3+); Macs are covered through MoltenVK. Efficiency is the first priority: a Raspberry Pi 4–class board is our reference for typical low-end hardware, not a floor.

It has the same requirements and scope as its sister projects, [GBTGhoulies--QilinRecomp](https://github.com/Qilin1066/GBTGhoulies--QilinRecomp) (the same console, and this project's model), [PunchOutWii--QilinRecomp](https://github.com/Qilin1066/PunchOutWii--QilinRecomp), [RhythmHeavenFever--QilinRecomp](https://github.com/Qilin1066/RhythmHeavenFever--QilinRecomp), [ApbArcade--QilinRecomp](https://github.com/Qilin1066/ApbArcade--QilinRecomp), [WindWakerHD--QilinRecomp](https://github.com/Qilin1066/WindWakerHD--QilinRecomp) and [ToyCommander--QilinRecomp](https://github.com/Qilin1066/ToyCommander--QilinRecomp), down to the same settings menu.

> **No game data is included, and none ever will be.** You need your own copy of the game. The tools read your disc image, or the files extracted from it, from a path you give them. `.gitignore` keeps game files, and anything generated from them, out of the repo.

## Status

**It runs the game's opening scenes, not yet the whole game.** Every function the game needs lifts to C++ and compiles, and `qilin_run` runs the result over a native kernel, Direct3D (over Vulkan, with fixed-function lighting), DirectSound and the controller library. From power-on the game shows its logos and intro movie, its menus, the New Adventure movie, the stable yard, the dressing room and the stables in 3D, and its four mini-games through to their results (v0.9.1); that much has been driven with scripted controller input and runs without a stop. With `--virtual-time` (v0.10.0) such a run goes the same way every time, frame for frame. Every library function the game is known to call is provided (v0.11.0). Nothing has been played through yet. Every lifted function has been checked against the original x86 under an emulator, on random states (v0.12.0: no mismatch left). The recompiler core and the console-level runtime come from GBTGhoulies--QilinRecomp, stripped of everything that described that game.

| Piece | State |
|---|---|
| Disc image reader (the Xbox DVD file system; a full-disc dump or the game partition alone) | ✅ tested on synthetic images, malformed ones included; `qilin disc`, `qilin extract` |
| XBE loader (headers, certificate, sections, libraries, kernel thunks, flat image), the build recognised by SHA-256 | ✅ reads the game; the supported build is recorded |
| x86 decoder (Zydis, vendored) | ✅ agrees with LLVM on all 500,668 reachable instructions (v0.11.0) |
| Function discovery and control flow (recursive descent, switch tables, C++ exception and SEH tables, data-as-code validation), byte coverage | ✅ 7,651 functions; every byte of the lifted regions accounted for (v0.11.0); 73 indirect jumps unresolved, all run-time targets |
| Lifter, x86 → C++ (integer, scalar SSE, x87 with precision control, jump tables, string operations, calls, C++ exception frames) | ✅ all 6,772 functions lift (the game and its engine whole, the libraries by reach); the output compiles |
| What the analysis has established about the game (`recomp/src/game.hpp`) | ✅ the build's identity, the regions of `.text`, alignment, the C runtime's conventions, the native functions (`docs/code-analysis.md`) |
| Differential test of every lifted function against the original x86 | ✅ 6,772 functions, 40,632 trials, 0 mismatches (v0.12.0; it found one lifter bug, in the C runtime's `exp`) |
| Renderer (Vulkan 1.3), audio engine, window, controllers, sound output (SDL3) | ✅ carried over from GBTGhoulies--QilinRecomp in v0.2.0, with their tests; driven by the game since v0.8.0 |
| The settings menu shared with the sister projects, and the settings file | ✅ carried over, under this game's name; `qilin_menu_preview` shows it over a test card |
| Runtime: the kernel, the scheduler and the file system, natively; `qilin_run`, which starts at the lifted entry point | ✅ the game starts on it and runs into its engine (v0.6.0); three test programs go from XBE through the lifter to native programs and run in CI |
| The controller library | ✅ natively, found by name in this XDK build, through the window |
| DirectSound | 🔨 natively over the mixer (v0.7.0), found by name; the movie's sound plays; 3D positioning by the library's own calculator, lifted (v0.11.0) |
| Direct3D | 🔨 natively over the renderer (v0.8.0), with fixed-function lighting (v0.9.0): the menus, the stable yard, the dressing room, the stables and the mini-games; specular and two-sided lighting (v0.11.0) |
| The XMV movie player | ✅ lifted whole, through the video overlay (v0.8.0) |
| Enhancements wired to the game (widescreen, the HUD, frame rate) | to do, after the game plays at original spec |

## Versions and changes

Every release is recorded in [`CHANGELOG.md`](CHANGELOG.md). Past entries are kept, never rewritten. The version lives in one place, `project(... VERSION ...)` in `CMakeLists.txt`, and `qilin --version` prints it. While the recompiler is being built, a minor bump (0.**x**) marks a milestone and a patch bump (0.x.**y**) a fix. 1.0.0 will be the first build that plays the game.

## Analysing your copy (Windows, double-click)

With Visual Studio 2022 (17.10 or newer, the free Community edition, with "Desktop development with C++") and Git installed, double-click **`Analyse game.bat`** in the project's folder. It builds the `qilin` tool, asks for your disc image (or `default.xbe` in an extracted disc's folder) and writes what it finds to `analysis-out\`: the disc's file list, the executable's summary and SHA-256, its kernel imports, its functions, the byte accounting. That folder is derived from the game and is never committed.

## Building

The whole project is **C++23**. You need CMake ≥ 3.25 and one of:

| Compiler | Minimum | Notes |
|---|---|---|
| GCC | 14 | Debian 13 / Raspberry Pi OS (Trixie) and Ubuntu 24.04 (`g++-14`) have it |
| Clang | 19 | Older Clang can't use libstdc++'s `std::expected` |
| Apple Clang | Xcode 16 | |
| MSVC | Visual Studio 2022 17.10 | |

CMake stops with a clear message if the compiler is older.

The one other dependency is **SDL3** (window, input, audio output). CMake uses an installed SDL3 (3.2 or later) if it finds one; otherwise it downloads SDL 3.4.20 (checked against its SHA-256) and builds it static, which needs network access once. On Linux, SDL needs the development headers of what it supports, or the build has no window or sound there. On Debian, Ubuntu and Raspberry Pi OS:

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

CI builds and tests with warnings as errors on Linux x64 (GCC 14 and Clang 19), Linux ARM64 (GCC 14), macOS ARM64 and x64 and Windows x64 (MSVC).

## Using it

`<game>` is your copy: a disc image, an extracted disc's folder, or its `default.xbe`.

```sh
build/qilin info     <game>                     # header, sections, XDK libraries, kernel imports (JSON), SHA-256
build/qilin imports  <game>                     # the kernel functions it imports, by name
build/qilin disc     <image>                    # the files on a disc image
build/qilin extract  <image> --out DIR [--only PATH]   # outside the repo, or an ignored folder
build/qilin cfg      <game> --json cfg.json     # function discovery + control flow
build/qilin align    <game>                     # how directly-called functions are aligned
build/qilin suspects <game>                     # kept functions with problems, for review
build/qilin coverage <game> [--list]            # every byte: code, table, padding or unknown (exit 3 if lifted code has unknown bytes)
build/qilin insns    <game> [--linear] -o listing.txt
build/qilin lift     <game> --out gen/          # lift to C++ (gen/ is gitignored: never commit it)
build/qilin_run      <extracted disc> [--data-dir DIR] [--ram-mb N]   # run it (built with -DQILIN_GEN_DIR=gen)
```

`build/qilin_menu_preview` opens the game's window with a test card and the settings menu (F1 or Esc), with no game needed: the menu, the picture settings and the keys can be tried, and they are kept in the real settings file.

Code that compiles lifted C++ must link the `qilin_runtime` CMake target (or pass `-ffp-contract=off`): SSE results are only bit-exact without fused multiply-add.

## Layout

| Path | What |
|---|---|
| `recomp/src/` | Recompiler core: `xiso` (disc images), `xbe` loader, `sha256`, `decoder` (Zydis wrapper), `cfg` and `vrange` (function discovery, control flow, jump-table sizes), `lifter` (x86 → C++), `game.hpp` (the supported build's facts), `main` (the CLI) |
| `recomp/runtime/` | Runtime headers the generated C++ includes (guest memory, flag and float helpers, x87 maths) |
| `recomp/runtime/gfx/` | The renderer: Vulkan 1.3, loaded at run time |
| `recomp/runtime/audio/` | The audio engine: voices (PCM, Xbox ADPCM) and the 48 kHz mixer |
| `recomp/runtime/platform/` | The window, the controllers and sound output, through SDL3; the settings file |
| `recomp/runtime/ui/` | The settings menu, and `qilin_menu_preview` |
| `recomp/runtime/host/` | The runtime's host side: `qilin_run`, the replacement registry, guest RAM, the kernel, the scheduler, the file system |
| `recomp/tests/` | Unit tests on a synthetic in-memory XBE and disc image, and the hand-assembled test programs the end-to-end tests lift, build and run; none needs game files |
| `tools/check/` | The LLVM decoder cross-check and the x87 rounding and transcendental cross-checks |
| `tools/analyse-game.ps1` | What `Analyse game.bat` runs |
| `third_party/` | Vendored dependencies: Zydis and Zycore, Dear ImGui, FSR 1, SMAA and volk (MIT), the Vulkan headers (Apache-2.0 or MIT), nxdk's kernel export list (CC0). See `third_party/README.md` |
| `docs/` | Design notes: the roadmap and every design target, the recompiler core, naming, prior art, the analysis (pending) |

## Licence

This project's own code and documentation are under the [GNU General Public License, version 3](LICENSE) (GPL-3.0), as in the other QilinRecomp projects: you may use, share, change and sell it, but anything you distribute that is built from it must come with its full source under the same licence. The vendored code in `third_party/` keeps its own licences, listed in `third_party/README.md`. SDL3, fetched by CMake at build time, keeps its zlib licence.

The game itself belongs to its owners (Mattel, Vivendi Universal Games, Blitz Games) and is not covered by anything here: you need your own copy, and nothing built from it may be shared.

## Design targets (short version)

- **Efficiency and reliability come first.** Every choice is judged by whether it makes the game cheaper to run on low-end hardware, and whether it fails loudly instead of misbehaving quietly.
- **Static recompilation, x86 → C++, compiled ahead of time.** No interpreter and no JIT. Hand-written C++ can override any lifted function.
- **No DirectX.** Direct3D 8 is translated at API level to Vulkan 1.3, using a deliberately small baseline feature set (1.3 core plus what the Pi 4's driver offers) so one render path runs on everything from low-end ARM GPUs upwards. macOS runs through MoltenVK.
- **The kernel and the XDK libraries that drive hardware are replaced natively**, not recompiled. The C runtime and other pure-computation libraries are lifted with the game, for exact behaviour.
- **Ground truth is xemu** running the original game.

See `docs/` for the full roadmap.

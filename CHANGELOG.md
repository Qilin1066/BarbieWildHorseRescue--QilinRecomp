# Changelog

All notable changes to this project are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/). While the recompiler is being built, a minor bump (0.**x**) marks a milestone and a patch bump (0.x.**y**) a fix. 1.0.0 will be the first build that plays the game. Past entries are never rewritten.

## [0.3.0] - 2026-11-21

The kernel, and programs that run: an Xbox executable goes through the lifter to a native program over the native kernel. Still nothing from the game: every check is on test programs.

### Added
- The runtime's kernel-level host side, from GBTGhoulies--QilinRecomp v0.41.3 (`docs/runtime.md`): the replacement registry, guest RAM, the kernel (80 exports: memory, critical sections, timers, DPCs, settings, sections, strings, SHA-1), the scheduler (guest threads on host threads), and the I/O manager over an extracted disc and a data folder, with its tests.
- `qilin_run` (`recomp/runtime/host/run.cpp`): loads the executable, sets up what the console's kernel does before a title runs, and calls the XBE's **entry point**, lifted. No address of the game is needed to start it (the sister project's calls its game's start-up functions by address). It refuses an executable that is not the supported build, once that is recorded.
- End-to-end tests in CI (`recomp/tests/make_test_xbe.cpp`, `qilin_add_program`): three hand-assembled Xbox programs written as XBE files, lifted, built and run as the game will be: one calls the kernel and computes, one reads a file from the disc, one calls a kernel function the runtime lacks and must be stopped by name.
- `tools/gen/xdk_symbols.py` (XbSymbolDatabase names for the game's XDK libraries); `recomp/runtime/host/xdk_symbols.inc` is empty until it has been run on the game.

### Changed
- What the runtime may know of the supported build (its name, SHA-256 and code regions) is `recomp/src/build.hpp`; `game.hpp` keeps the tools' part. Finding `default.xbe` in an image, a folder or a file is `recomp/src/game_file.*`, shared by the tools and the runtime.
- The window, sound output and audio engine brought up to the sister project's v0.41.3 (its sound log).

## [0.2.1] - 2026-11-21

### Changed
- The repository is now **BarbieWildHorseRescue--QilinRecomp** (the owner renamed all the QilinRecomp repositories on 2026-11-21: `<Game>--QilinRecomp`). The documents, the CMake project and the links to the sister projects use the new names; earlier entries below keep the names of their day (BeatTheBeat-QilinRecomp is now RhythmHeavenFever--QilinRecomp).

## [0.2.0] - 2026-11-21

The console-level half of the runtime, from the sister project, and the settings menu on a test card. Still nothing from the game: no copy of it has been available.

### Added
- The renderer (`recomp/runtime/gfx/`): Vulkan 1.3 through volk, NV2A vertex programs and pixel combiners as generated SPIR-V, textures, the present pass with whole-multiple, fit and fill scaling, sharp or smooth pixels, AMD FSR 1, SMAA, the CRT look, vsync and low-latency pacing, render-resolution scaling, anisotropic filtering, the pipeline cache. From GBTGhoulies-QilinRecomp v0.41.2, unchanged; its tests run on Mesa's software driver in CI.
- The audio engine (`recomp/runtime/audio/`): PCM and Xbox ADPCM voices, the 48 kHz mixer, reverb. Unchanged, with its tests.
- The platform layer (`recomp/runtime/platform/`): the window, controllers and the keyboard, sound output, through SDL3; and the settings file, `wildhorserescue.ini`, key for key the sister project's.
- The settings menu (`recomp/runtime/ui/`, Dear ImGui): the same menu as the other recomps, item for item GBTGhoulies-QilinRecomp's, under this game's name.
- `qilin_menu_preview`: the window, the present pass and the menu over a test card, reading and writing the real settings file, so the menu and the picture settings can be tried before the game runs. CI runs it for 30 frames with the menu open on a virtual display.
- Tests for the settings file: what is written reads back the same, every key and control is there, unreadable lines are named and left at their defaults.
- `platform::default_data_folder()`: `<per-user data>/qilin/wildhorserescue`.

### Changed (from the sister project's code)
- The language setting accepts every console language (1 to 6) and the menu lists them: which the disc has is not known yet. The list is cut to the disc's before the menu reaches a player.
- The frame-rate setting's first choice reads "Original (the console's)": whether the game runs at 30 or 60 is not known yet.
- The `[console] widescreen` key (a setting of that project's v0.36) is not read.

### Fixed (from the sister project's code)
- `channels = 4` (or any refused number from 0 to 6) was reported as left at its default but kept. It is now left at its default.

## [0.1.0] - 2026-11-21

The start of the project: the tools that read the game. Nothing runs yet, and the tools have not read the game yet: no copy of it was available, so every check is on synthetic files.

### Added
- **Licence: GPL-3.0** (`LICENSE`), as in all the QilinRecomp projects.
- The project's requirements, scope and roadmap, the same as the other Qilin recomps' and applied to this game (`docs/roadmap-and-design-targets.md`), with the settings menu's layout, the naming rules (`docs/naming.md`) and the prior art (`docs/prior-art.md`).
- The recompiler core of GBTGhoulies-QilinRecomp v0.41.1, the sister project on the same console (`docs/recompiler-core.md`): the XBE loader, the Zydis decoder (vendored, v4.1.1), function discovery and control flow with jump-table sizes proven, byte coverage, the lifter (integer, scalar SSE, x87 with precision control, string operations, calls, C++ exception frames), and the runtime headers lifted code compiles against.
- `recomp/src/game.hpp`: the one place for what the analysis establishes about the supported build (its SHA-256, the regions of `.text`, what is lifted, replaced or native, function alignment, conventions). Every table is empty: nothing is carried over from the other game. With nothing recorded the tools take the whole of `.text` as one lifted region and assume no alignment.
- A reader for Xbox disc images (`recomp/src/xiso.*`): the game partition at its offset in a full-disc dump or at the start of the file, the volume descriptor, the directory trees, every table validated. `qilin disc` lists an image and `qilin extract` copies files out of it; every other command takes the game as an image, an extracted folder or `default.xbe`.
- `qilin info` prints the executable's SHA-256 and whether it is the supported build; once the hash is recorded, another build is refused (exit 4) unless `--any-build` is given.
- `qilin imports`: the kernel imports by name (nxdk's export list, CC0); an ordinal the kernel doesn't export is an error.
- `qilin align`: where directly-called functions start, modulo 16, per code section.
- `Analyse game.bat` (Windows): builds `qilin` and writes the first analysis of your copy to `analysis-out\`. Not yet run.
- Tests without game files: the unit tests, the disc reader on well-formed and malformed images, and the `qilin` tool run on the synthetic XBE.
- CI on six configurations (Linux x64 with GCC 14 and Clang 19, Linux ARM64, macOS ARM64 and x64, Windows x64), warnings as errors, compiler messages as annotations.

### Changed (from the sister project's code)
- The coverage unit test pins the exact accounting of the synthetic `.text`; before, its region didn't fit the section and no unknown byte was ever counted.

[0.1.0]: https://github.com/Qilin1066/BarbieWildHorseRescue--QilinRecomp/commits/main

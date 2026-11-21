# Changelog

All notable changes to this project are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/). While the recompiler is being built, a minor bump (0.**x**) marks a milestone and a patch bump (0.x.**y**) a fix. 1.0.0 will be the first build that plays the game. Past entries are never rewritten.

## [Unreleased]

## [0.9.0] - 2026-11-21

Into the game: from power-on through the menus, the New Adventure movie, the stable yard, the dressing room and the stables, and through the Mini-Games lobby into a race, drawn in 3D with lighting, with no stop in any of it.

### Added
- `qilin_run --press SCRIPT`: controller presses on port 1 by frame shown (`FRAME:BUTTONS[/FRAMES],...`, e.g. `2300:start/10,2700:a/10`), for runs with no one at the controls. With it the game goes from the title screen through the main menu, the New Adventure movie and loading, into the stables yard and the dressing room, the stables (choosing a horse) and riding round the yard; and through the Mini-Games lobby into a race (Silver Valley Gallop), three laps against Christie. Thumbsticks (`lu ld ll lr`, `ru rd rl rr`) as well as buttons.
- **Fixed-function lighting**: directional, point and spot lights, the four colour sources (the material's or the vertex's colours with `D3DRS_COLORVERTEX`), global ambient, attenuation and range, the spot cone, normals through the inverse transpose of world × view, `D3DRS_NORMALIZENORMALS`; generated into the fixed-function vertex program. Specular and two-sided lighting still stop the runtime.

### Fixed
- **Threads started with a zeroed x87 control word**: `qilin_run` set the first thread's (`0x027F`) only after the scheduler had copied its registers as the start of every later thread and of DPCs, so those ran with every floating-point exception unmasked and single-precision rounding. The C runtime's `floor`/`ceil` then raised an inexact-result exception on the main thread as the menu opened. (The sister project's runner has the same order.)
- **Depth testing was off for the whole game**: Direct3D's default state sets `D3DRS_ZENABLE` to whether the device has an automatic depth buffer, which this build reads from the device's `+0x1A10`; the layer, written for another XDK build, never filled it. Barbie was painted over by the room in the dressing room.
- **The frame buffers flip as this build does it**: its `Swap` (`0x174600`) keeps each surface object in its role (back, front) and moves the memory along, where the layer (after the sister's build) moved the roles. The game keeps the back buffer's surface and sets it as its render target every frame, which then pointed at the front buffer.
- **Function discovery lost a real function behind a false one**: a data word seeded a function in the middle of an instruction of the real one at `0x800C0`, which the mini-games call through a pointer; the run stopped there with no lifted code. Immediate pointers are now always kept as seeds, and an accepted one evicts weakly-evidenced functions whose entries its own instructions step over. One function changes; the game region's unknown bytes go from 67 to 23 (`docs/recompiler-core.md`).
- The performance overlay and `QILIN_FPS` read the sister game's time step at its address; they no longer do (this game's isn't located).

## [0.8.0] - 2026-11-21

Pictures: Direct3D over Vulkan, the window and the settings menu. The game runs from power-on through its logos and intro movie (with sound) to its title screen, "Press START to begin", and stays there, animated.

### Added
- **Direct3D natively** (`recomp/runtime/host/d3d.cpp`, from GBTGhoulies--QilinRecomp v0.41.3) over the Vulkan renderer, with the window, controllers through it and the settings menu wired into `qilin_run` (`--headless`, `--fullscreen`, `--dump-frames`, `--frames`, `--gpu`). The game draws its start-up screens: the legal notice and the three publishers' and developer's logos, then the intro movie.
- Every address the layer took from the sister build and that hasn't been found in this one stops the runtime if reached, naming it; replacements are registered by name, each one's argument size checked against this build's code (`xdk_ret_bytes.inc`, `tools/gen/xdk_ret_bytes.py`).
- Direct3D's resource code (textures, surfaces, locks, descriptions), its default state, its shader-object builders, its material and light state and their helpers, lifted: about 40 functions found in this build.
- What this game needs that the sister game didn't: `Swap` with `D3DSWAP_COPY` and this XDK's default flags; fixed-function texture transforms; normals in unlit draws; a stage reading a texture with none set.
- **The intro movie plays**: Microsoft's XMV decoder (the XMV section) is lifted whole, reading the disc through XAPI, decoding into Direct3D surfaces and playing its sound through a DirectSound stream; its pictures reach the screen through the video overlay (`UpdateOverlay`, `EnableOverlay`, `GetOverlayUpdateStatus`: YUY2 to RGB, scaled, colour-keyed), its timing through `GetDisplayFieldStatus` and `GetDisplayMode`.
- The lifter: the MMX comparisons, averages, minimum and maximum, saturating additions and subtractions and `pmulhuw`, and `bswap`, with unit tests. The CFG: a jump through a register the function loads only with its own labels (hand-written code's local subroutines, `mov ebx, label; jmp sub` … `jmp ebx`) becomes a switch over those labels; a function at the first byte of a code section can be found by an immediate pointer to it.
- The front end's needs: palettes (`SetPalette`, `D3DPalette_Lock2`), `SetShaderConstantMode`, `D3DResource_BlockUntilNotBusy`.
- DirectSound: `DirectSoundCreateStream`, this build's stream vtables, `Pause(2)` and `SynchPlayback` (streams started together).
- User-mode APCs: `NtQueueApcThread`, delivered when a thread waits alertably in user mode (the engine's loader thread runs on them); reads and writes with an event or a completion routine; XAPI's `NtUserIoApcDispatcher`; `MmSetAddressProtect`.
- `docs/graphics.md`: what bringing the sister project's Direct3D layer to this XDK build has established (the device object's layout is the same, the state tables moved together, the functions are in another order and not all identical), the helpers identified so far, and the plan.

### Changed
- Direct3D's object creation (`0x174900`) and its two capability queries (`D3D_GetDeviceCaps`, `D3DDevice_GetDeviceCaps`) are lifted: pure functions.

### Not yet
- Nothing past the title screen has been tried: in a headless run there is no controller to press START with. Fixed-function lighting stops the runtime when a lit draw comes; the frame rate and picture shape settings have no effect until the engine's frame clock and projection are found; nothing of the lifted code has been checked against the original x86 yet.

## [0.7.0] - 2026-11-21

DirectSound: the engine's sound system starts on the native mixer, and the run reaches Direct3D.

### Added
- DirectSound natively (`recomp/runtime/host/dsound.cpp`, from GBTGhoulies--QilinRecomp v0.41.3) over the audio engine, its functions found by name in this XDK build, each one's argument size checked against this build's code. `qilin_run` gains `--mute`, `--dump-audio` and `--audio-channels`.
- What this game uses that the sister game didn't: play-cursor notifications (`SetNotificationPositions`: events signalled as the cursor passes each offset, or when the buffer stops), `GetCaps`, `GetSpeakerConfig`, `DirectSoundUseFullHRTF`, and DirectSound's own 3D calls (`SetPosition`, `SetVelocity`, `SetMinDistance`, `SetMaxDistance`, `SetRolloffCurve`, the listener's `SetAllParameters`).

### Not yet
- **3D positioning is recorded, not applied:** 3D sounds will play unpositioned (the runtime says so once). The sister project lifts the library's own 3D calculator; the same is to do here.
- Nothing has been heard: the run stops in the engine's display start-up, before any sound plays. The notification code is untested against the game.
- `IDirectSound_DownloadEffectsImage` gives a null effect description, which this game asks for and never reads.

## [0.6.0] - 2026-11-21

The game runs through start-up into its engine, up to the sound system: the stop is now `DirectSoundCreate`, by name.

### Added
- The XDK library functions named (`recomp/runtime/host/xdk_symbols.inc`, 322 symbols of XDK 5558, by XbSymbolDatabase): the runtime's stops and the list of what lifted code calls in the replaced libraries now read as names (`docs/code-analysis.md`: 48 Direct3D functions, 27 DirectSound, 6 XMV, 2 memory-unit calls).
- The controller library natively (`recomp/runtime/host/xinput.cpp`, from GBTGhoulies--QilinRecomp, found by name in this build): `XInitDevices`, `XGetDevices`, `XGetDeviceChanges`, `XInputOpen`, `XInputClose`, `XInputGetState`, `XInputSetState`. Memory units, microphones and headphones, which the engine also asks about, are reported as not plugged in. No window yet, so every port is empty.
- Kernel: `RtlTimeToTimeFields` and `RtlTimeFieldsToTime` (tested), the settings' whole user section (`ExQueryNonVolatileSetting` index `0xFF`), named events and mutants (creating one that exists opens it), and the Win32 error for a write-protected volume.
- The replaced libraries' seven static initializers are lifted, so the C runtime's start-up runs its tables whole.

## [0.5.0] - 2026-11-21

Every function lifts, the lifted game compiles, and it starts: XAPI's entry point runs natively and creates the title's main thread, which runs to its first call into a hardware library.

### Added
- The regions of `.text` (`recomp/src/build.hpp`, evidence in the new `docs/code-analysis.md`): the game, XAPI, Blitz Games' engine ("Babel"), the C runtime with the rest of XAPI, and the exception-handling funclets. The game and the engine are lifted whole; the libraries function by function from the entry point.
- The C runtime's conventions and the four functions replaced natively (`game.hpp`; `recomp/runtime/host/game_native.cpp` registers three of them), and the runtime's time-stamp counter (`tsc.cpp`).
- Lifter: the one-operand 8-bit multiply, `xlat`, `sfence`, `fstp m80` and `fprem` (`rt::fprem`, tested).
- `qilin_run` keeps its first thread after the entry point returns, as the host's housekeeping thread, so the threads the entry point created run on (`QILIN_EXIT_AFTER_ENTRY=1` ends the process there instead, for the test programs).

### Changed
- **6,621 functions lift, all of them**; the game region is 99.992% accounted for (67 unknown bytes), the engine and the funclets 100%.
- Function discovery: reading a segment register (`mov`, `push`) marks data decoded as code; gaps are seeded again after validation drops a false seed.
- The game's facts apply only to the supported build: an executable read with `--any-build` gets none of them.

### Fixed
- **Every controller works the F1 menu**, not only the first one SDL lists (which can be a virtual pad or a wheel's base): Dear ImGui's SDL3 backend is set to read them all, as in the other QilinRecomp projects.


## [0.4.0] - 2026-11-21

The first analysis: the tools have read the game.

### Added
- `docs/xbe-analysis.md`: the executable's identity, memory layout, sections with their plans, XDK libraries (5558), kernel imports against what the runtime provides, the first function discovery (7,659 functions, 97.958% of `.text` accounted for), function alignment by range, the first lift (5,844 of 5,899 functions in `.text`) with the failures by reason, and what the disc holds.
- The supported build's SHA-256 (`recomp/src/build.hpp`): the tools and `qilin_run` now refuse any other executable unless given `--any-build`. A test checks the refusal.

### Changed
- `.data1` and `sig` are data sections to the loader (they were swept as code).
- The settings menu's languages are the console languages the disc has tables for: English, German, French, Spanish, Italian (Japanese is refused again in the settings file).

### Fixed
- `qilin lift` never ended when a secondary entry inside a lifted function failed to lift: it was tried again for ever. Each is tried once and reported. (The sister project's code has the same loop; there every such entry lifts.)

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

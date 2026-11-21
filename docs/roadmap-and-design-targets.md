# Barbie Horse Adventures: Wild Horse Rescue — Recomp Roadmap & Design Targets

The requirements and scope are those of the sister projects (GBTGhoulies-QilinRecomp, ApbArcade-QilinRecomp, BeatTheBeat-QilinRecomp, PunchOutWii-QilinRecomp, WindWakerHD-QilinRecomp, ToyCommander-QilinRecomp), unchanged, down to the settings menu. GBTGhoulies-QilinRecomp is the same console and is the model: where a design question here is undecided, do what it did.

**Guiding principle: efficiency and reliability come first.** Every design choice is judged on whether it makes the game cheaper to run on low-end hardware and whether it fails loudly and predictably instead of silently misbehaving.

## Targets
- **Game:** Barbie Horse Adventures: Wild Horse Rescue (Blitz Games / Vivendi Universal Games, original Xbox, 2003), the **USA release**. The supported build is identified by its `default.xbe`'s SHA-256 once the first analysis records it (`xbe-analysis.md`). Other releases come after this one plays.
- **Platforms:** 64-bit Windows, Linux and macOS (Intel and Apple Silicon), on x86-64 or ARM64, with Vulkan. See *Hardware scope* below.
- **Language:** **C++23** throughout (recompiler, generated code and runtime). Minimum compilers: GCC 14, Clang 19, Xcode 16, Visual Studio 2022 17.10. No DirectX anywhere.
- **Graphics API:** **Vulkan 1.3, and only Vulkan.** We require the 1.3 core API, and only the optional features and limits that Mesa's v3dv driver on a Pi 4 provides, so one render path works everywhere (MoltenVK on macOS). See *Vulkan baseline* below.
- **Reference:** xemu running the original game is ground truth for behaviour and visuals. It is run, never read into this code.
- **Enhancement example target:** 3440×1440 (21:9) at 144 Hz.

## Architecture
What follows is the sister project's architecture. The parts that depend on what this game's executable contains are marked **to confirm**: they are that project's answers, to be checked against this game in step 1, not assumed.

- **Game code:** static recompilation, x86 → **generated C++**, compiled ahead of time by GCC, Clang or MSVC for each target. No interpreter, no JIT, no emulator at run time, not even as a fallback. Hand-written C++ replaces lifted functions over time (hot paths, bugs, enhancement hooks) through a function override table.
- **Decoder:** **Zydis** (MIT), vendored, and **verified instruction by instruction against LLVM's disassembler across the whole binary**.
- **XDK symbol naming:** XbSymbolDatabase (MIT) to name the D3D8, DirectSound, XAPI and C runtime functions; own masked-hash signatures only where it lacks the game's XDK build.
- **Register model:** guest registers held in host locals inside each lifted function and synced to a context struct only at call and indirect boundaries. No global or thread-local register variables.
- **Calling conventions:** lifted code keeps full register state across calls and never assumes cdecl or stdcall (code of this compiler generation built with link-time code generation passes arguments in arbitrary registers). ABI recovery only where hand-written C++ replaces a function.
- **Fail loudly:** an undecodable or unsupported instruction is a build error, never a comment; an unknown indirect-call target at run time traps with its address logged; an executable that is not the supported build is refused.
- **Control flow:** recursive-descent function discovery with resolved switch tables, C++ exception tables and SEH scope tables read, and data decoded as code rejected. Every byte of the lifted code is accounted for: code, table, padding or unknown, and unknown must reach zero (`qilin coverage`). Indirect calls dispatch through a generated sorted function table.
- **Lifter efficiency:** flags computed only where they are read; scalar SSE → native `float` (bit-exact on ARM); x87 → native `double`, honouring the game's x87 precision-control setting (**to confirm** which it sets), with transcendentals from a bundled implementation, correctly rounded and the same on every host; flat memory (4 GB host reservation, `base + addr`, no bounds checks in the release build). Both sides are little-endian.
- **Memory mirrors:** the Xbox maps RAM at several aliases. Guest RAM is one shared-memory object mapped at 0 and at each alias the game uses (**to confirm** which: the sister game uses `0x80000000` and `0xF0000000`), inside one 4 GB host reservation; any other address outside RAM traps in the checked build.
- **Exceptions:** C++ exception frames kept in guest memory exactly as the original keeps them, each guest thread with its own TIB. Whether anything in the game catches an exception it can throw decides whether unwinding is implemented or a throw stops with its type and message (**to confirm**).
- **Kernel + XDK libraries:** the kernel and the libraries that drive hardware (Direct3D 8, DirectSound, the controller library) are **replaced natively**, not recompiled. Libraries that are pure computation over an interface we replace are **lifted** with the game, the functions lifted code reaches: the C runtime, XAPI, and (where the game links them) XACT, XGraphics, D3DX and the WMA decoder. Exact `printf` formatting, x87 maths and heap behaviour matter more than native speed. The boundary is drawn library by library in `code-analysis.md` once the analysis has named them.
- **Graphics:** Direct3D 8 translated to Vulkan at its API, not NV2A emulation; plus a minimal NV2A command interpreter only if the game writes raw push-buffer commands itself (**to confirm**), where an unknown method is fatal. Vertex programs and pixel combiners become generated SPIR-V; pipelines come from a cache on disk, made again on a thread at start (no run-time compile stutter).
- **Video:** whatever the disc holds (**to confirm**: Bink, XMV or none). A decoder linked into the executable is lifted with the game; frames are uploaded as Vulkan textures.
- **Audio / input:** DirectSound → native voices and a software mixer (PCM and Xbox ADPCM, mix bins, streams), output on SDL3; XACT and the WMA decoder lifted if present; controllers through SDL3 (an Xbox pad over Bluetooth is the test controller), with the keyboard on player 1 and rumble.
- **Saves and the disc:** the file system over the player's own disc (an image, read directly, or an extracted folder) and a data folder for saves.
- **Threads:** audio mixing on its own thread, the GPU one submission behind the CPU; guest threads on host threads with the Xbox's single-processor scheduling.
- **Portability rules:** no inline assembly or x86 intrinsics in the runtime; ARM64 Linux builds from day one.

## Hardware scope
**Supported: 64-bit hosts with a Vulkan 1.3 driver.** In practice that means:
- **Windows and Linux** on x86-64 or ARM64 with a Vulkan-capable GPU and driver;
- **macOS on Intel and Apple Silicon Macs**, through MoltenVK (Vulkan on top of Metal): any 64-bit Mac with a Metal GPU running macOS 10.15 (Catalina) or later. The runtime (what players run) is built for macOS 10.15 and so doesn't use `std::format`, which Apple's libc++ only has from macOS 13.3; it formats with `qilin::strf` (printf-style). The developer tools (`qilin`, the tests) use `std::format` and need macOS 13.3;
- **ARM boards** with Vulkan 1.3 drivers, e.g. Raspberry Pi 4 and newer (Mesa v3dv, Mesa 24.3 or later).

**Out of scope:** anything without Vulkan 1.3 (or, on Apple, a translation layer that provides it), and 32-bit hosts. Because of this there is one renderer (Vulkan) and one memory model: the Xbox's 4 GB address space mapped flat into a 64-bit host's.

### Vulkan baseline: 1.3
The renderer targets **Vulkan 1.3** as its minimum API version: Mesa's v3dv on the Raspberry Pi 4 and 5 is 1.3 conformant from Mesa 24.3, MoltenVK supports 1.3 from v1.3.0, and current desktop drivers all ship it. The renderer is designed around what 1.3 makes core: dynamic rendering (it suits Direct3D 8's free-form render-target switching), synchronization2, extended dynamic state (fewer pipelines for Direct3D 8's render states), timeline semaphores for pacing.

1.3 core is the floor, but optional features and device limits aren't: v3dv is the reference for those. The renderer checks the features it uses at start-up and fails with a clear message if one is missing, rather than taking a fallback path. Nothing beyond 1.3 core and the v3dv feature set is used without a decision recorded here.

**Low-end reference:** a Raspberry Pi 4–class board is our reference for typical low-end hardware. It's what we measure efficiency against and test on. It is not a floor.

## Settings, not platform branches
All enhancements live behind a settings file (`wildhorserescue.ini`: `[display]`, `[controls]`, `[sound]`, `[console]`), written with every key and comments the first time the game runs, with presets, e.g.:
- *Low-end:* 1× resolution, no AF, original aspect and frame rate.
- *Desktop:* scaled resolution, 16× AF, anti-aliasing, widescreen, high refresh rate.

### The settings menu
The same menu as the other recomps, and on this console the same as GBTGhoulies-QilinRecomp's item for item (Dear ImGui; its `recomp/runtime/ui/menu.cpp` and `platform/settings.*` are the model, and the projects are kept in step):
- **Opening:** F1 or Esc; on a controller, Back + Start. It pauses the game and saves the settings on close. **Close** and **Quit the game** sit top right.
- **Controls:** the controllers found, rumble, every control of the Xbox controller set by pressing a key (A, B, X, Y, Black, White, both triggers, Start, Back, the D-pad, both sticks' four directions and their clicks; kept in `[controls]`), back to the defaults.
- **Display:**
  - *Window:* full screen (F11 or Alt+Enter).
  - *Picture:* whole multiples, fit, or fill the window; smooth (bilinear) or sharp pixels; SMAA anti-aliasing; AMD FSR 1 upscaling with its sharpness; plain or CRT television with scanlines and phosphor stripes.
  - Vsync; the frame rate (the console's own, 60, the display's refresh rate, or unlimited); low latency (with vsync, at most one frame waiting); frame times (F3: frames a second, the 1% and 0.1% lows, the worst frame, a graph); "look at the picture" (the menu hidden while held).
- **Game:**
  - *Enhancements:* aspect ratio (auto, 4:3 the original, and wider up to 4:1, Hor+); the HUD on wide pictures (at the screen's edges, in the middle 4:3, or stretched); render resolution (auto, or 1× to 4× the game's 480 lines); anisotropic filtering (1, 2, 4, 8, 16).
  - *Graphics device.*
  - *Sound and language:* volume; sound output (the device's own, stereo, 5.1: from the next start); the console's language (from the next start).
  - *About.*
- **Conventions around it:** the window is named after the game and opens at the largest whole multiple of 640×480 that fits; every gamepad is opened and their buttons added up; the data folder is `<per-user data>/qilin/wildhorserescue`.

Each enhancement arrives with its roadmap step; an entry is not shown before it works.

## Roadmap
1. **Analysis:** to do, and first: it needs the owner's copy (`xbe-analysis.md`). Headers, sections, kernel imports, XDK versions; function discovery, the CFG, library boundaries, function alignment and conventions; raw NV2A usage; the disc's files and asset formats.
2. **Recompiler core (C++23):** 🔨 the tools exist, carried over from GBTGhoulies-QilinRecomp v0.41.1 and not yet run on this game (v0.1.0; `recompiler-core.md`). ✅ XBE loader, ✅ disc image reader, ✅ the build's identity by SHA-256, ✅ Zydis decoder, ✅ function discovery and the CFG, ✅ byte coverage, ✅ the lifter (integer, scalar SSE, x87 with precision control, jump tables, string operations, calls, C++ exception frames), ✅ unit and tool tests without game files, ✅ CI with warnings as errors on Linux x64 (GCC 14, Clang 19), Linux ARM64, macOS ARM64 and x64, Windows. To do, on the game: the decoder cross-check against LLVM, coverage to 0 unknown bytes, every function lifting, the differential test of every lifted function against the original x86 (the sister project's `tools/difftest`, under Unicorn, brought over with this game's conventions).
3. **Runtime skeleton:** `qilin_run` loads the XBE into guest RAM, sets up the stack, the thread information block and the kernel imports, and calls the lifted entry point; a registry of native replacements that stops on anything unregistered, naming it and its caller; the kernel (memory, critical sections, timers, settings, the file system over the disc and a data folder); the scheduler.
4. **Boot to first frame:** the Vulkan 1.3 renderer, the Direct3D device, a window on SDL3.
5. **Playable at original spec:** 640×480, 4:3, original frame rate, sound, movies, saves, controllers; matches xemu; tested on low-end reference hardware at every milestone. The settings menu arrives here, as in the sister projects.
6. **Resolution / AF / anti-aliasing:** scaled render targets (decided per render-to-texture target), HUD half-pixel fixups, CPU frame-buffer read-back handled.
7. **Widescreen 16:9 → 21:9 and beyond:** the projection patched (Hor+), the culling frustum widened, the HUD re-anchored, 2D and movies pillarboxed, per-scene caps where the extra view shows unfinished geometry.
8. **High refresh rate:** the game's frame clock investigated first (fixed or variable step), then replaced natively: the console's rate, 60, the display's rate, unlimited, low-latency pacing from the swapchain; interpolation only if the logic is tied to a fixed step.
9. **Extras:** texture replacement packs (hash on upload), etc.

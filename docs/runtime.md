# The runtime

What runs beside the lifted code. The design is GBTGhoulies--QilinRecomp's (its `docs/runtime.md` is the long record: the scheduler's rules, the heap, the file system, each kernel function's behaviour and how it was checked); this file says what is here, what was changed, and what waits for the game.

## What is here (v0.3.0)
| Part | Files | State |
|---|---|---|
| The replacement registry: native functions at guest addresses; anything lifted code calls that has none stops the runtime, named, with its caller | `host/host.*`, `call_guest.cpp` | as the sister project's |
| Guest RAM: one shared-memory object mapped at 0 and at the kernel's two windows onto it, in a 4 GB reservation; checked accesses in bring-up builds | `host/guest_ram.cpp` | as the sister project's. Which windows this game uses is **to confirm** |
| The kernel: memory (virtual, contiguous, pool), critical sections, timers and DPCs, console settings, sections loaded on demand, strings, SHA-1 and HMAC | `host/kernel.cpp`, `kernel_crypto.cpp` | 80 of the kernel's exports; the game's imports are compared against them in step 1 |
| The scheduler: guest threads on host threads with the Xbox's single-processor rules, waits, events, mutants, DPCs | `host/sched.*` | as the sister project's |
| The I/O manager: `\Device\CdRom0` over the extracted disc (read-only, names matched without regard to case), the hard disk's partitions as folders of the data folder, symbolic links | `host/kernel_io.cpp` | as the sister project's |
| `qilin_run` | `host/run.cpp` | new: see below |
| The renderer, the audio engine, the window, input, sound output, the settings menu | `gfx/`, `audio/`, `platform/`, `ui/` | in since v0.2.0, not yet driven by a game |

Not here yet, because each is registered at addresses in the game's XBE and written against its XDK build: Direct3D, DirectSound, the controller library (XInput), XAPI's few native replacements, the game's own fixes, the aspect and frame-clock hooks. They follow the analysis.

## `qilin_run` starts at the entry point
The sister project's `qilin_run` replaces XAPI's start-up by hand: it calls that game's process initialisation, its initializer tables and its `main` at their addresses. That was written before its scheduler existed.

Here `qilin_run` does only what the console's kernel does before a title runs (RAM with the image in it, the sections not marked preload held back for `XeLoadSection`, the thunk table, the first thread with its stack, TLS block and information block) and then calls the XBE's **entry point**, lifted like the rest of the code. XAPI's start-up creates the title's main thread through `PsCreateSystemThreadEx`, which the scheduler provides, so no address of the game is needed to start it. What start-up code does that only a real console can (the sister game's XAPI patches kernel code and reloads the GDT on shutdown) shows as a stop naming the place, and becomes an entry in `game::kNative`.

**Confirmed on the game (v0.5.0):** the entry point creates the main thread (`0x0EA59B`) and returns 1. On the console the kernel's start-up thread ends there; `qilin_run` keeps its first thread, which gives the processor up for good and wakes every 10 ms for the host's housekeeping. It is the host's first thread, the one a window's events belong to on macOS, so it is where the window will be pumped from (the title's main thread is another host thread). `QILIN_EXIT_AFTER_ENTRY=1` ends the process when the entry point returns, for the test programs.

It reads an extracted disc (a folder, or the `default.xbe` in one). Given a disc image it says to extract it first (`qilin extract`); reading the image directly in the I/O manager is a later step.

## End-to-end tests without the game
`recomp/tests/make_test_xbe.cpp` writes small hand-assembled Xbox programs as XBE files. CMake lifts each with `qilin lift`, builds it with `run.cpp` and the runtime exactly as the game is built (`qilin_add_program`), and CTest runs it:

| Program | What it does | Passes when |
|---|---|---|
| `hello` | calls the kernel through its thunk table, loops, writes its BSS | the entry point returns 42 |
| `files` | opens `\Device\CdRom0\HELLO.TXT`, reads it, closes it | it returns the sum of the file's bytes |
| `missing` | calls a kernel function the runtime lacks | the runtime stops naming `DbgPrint`, its ordinal and the caller |

So the whole path, from an executable through the lifter and each CI compiler to a running native program over the native kernel, is exercised on every push. New kernel work gets a program here.

## Next
With the game: its kernel imports against the 80 provided; XbSymbolDatabase names for its XDK build (`tools/gen/xdk_symbols.py`); the first run, providing what each stop names.

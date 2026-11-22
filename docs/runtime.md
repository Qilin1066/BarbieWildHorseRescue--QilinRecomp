# The runtime

What runs beside the lifted code. The design is GBTGhoulies--QilinRecomp's (its `docs/runtime.md` is the long record: the scheduler's rules, the heap, the file system, each kernel function's behaviour and how it was checked); this file says what is here, what was changed, and what waits for the game.

## What is here
| Part | Files | State |
|---|---|---|
| The replacement registry: native functions at guest addresses; anything lifted code calls that has none stops the runtime, named, with its caller | `host/host.*`, `call_guest.cpp` | as the sister project's |
| Guest RAM: one shared-memory object mapped at 0 and at the kernel's two windows onto it, in a 4 GB reservation; lifted code accesses it unchecked, and an access to what isn't mapped faults, which a fault handler turns into the runtime's stop naming the guest address (v0.15.0; `-DQILIN_CHECKED_MEMORY=ON` bounds-checks every access instead, for bring-up) | `host/guest_ram.cpp` | as the sister project's. The game uses both kernel windows (D3D's contiguous memory and its write-combined surface locks) |
| The kernel: memory (virtual, contiguous, pool), critical sections, timers and DPCs, console settings, sections loaded on demand, strings, SHA-1 and HMAC | `host/kernel.cpp`, `kernel_crypto.cpp` | the sister project's exports and the ones this game added; anything else it calls stops the runtime, named |
| The scheduler: guest threads on host threads with the Xbox's single-processor rules, waits, events, mutants, DPCs | `host/sched.*` | as the sister project's |
| The I/O manager: `\Device\CdRom0` over the extracted disc (read-only, names matched without regard to case), the hard disk's partitions as folders of the data folder, symbolic links | `host/kernel_io.cpp` | as the sister project's |
| `qilin_run` | `host/run.cpp` | new: see below |
| The renderer, the audio engine, the window, input, sound output, the settings menu | `gfx/`, `audio/`, `platform/`, `ui/` | in since v0.2.0, driven by the game since v0.8.0 |

Since v0.6.0 the controller library is here too (`host/xinput.cpp`), and the game's own native functions (`host/game_native.cpp`); since v0.7.0 DirectSound (`host/dsound.cpp`: the sister project's `docs/audio.md` describes the mixer's design; what differs here is in the file's opening comment and the changelog). All are registered only for the supported build. Since v0.8.0 Direct3D (`host/d3d.cpp`, `docs/graphics.md`); the XMV player is lifted. Since v0.13.0 the widescreen and language hooks (`host/aspect.cpp`, `host/game_hooks.cpp`).

## `qilin_run` starts at the entry point
The sister project's `qilin_run` replaces XAPI's start-up by hand: it calls that game's process initialisation, its initializer tables and its `main` at their addresses. That was written before its scheduler existed.

Here `qilin_run` does only what the console's kernel does before a title runs (RAM with the image in it, the sections not marked preload held back for `XeLoadSection`, the thunk table, the first thread with its stack, TLS block and information block) and then calls the XBE's **entry point**, lifted like the rest of the code. XAPI's start-up creates the title's main thread through `PsCreateSystemThreadEx`, which the scheduler provides, so no address of the game is needed to start it. What start-up code does that only a real console can (the sister game's XAPI patches kernel code and reloads the GDT on shutdown) shows as a stop naming the place, and becomes an entry in `game::kNative`.

**Confirmed on the game (v0.5.0):** the entry point creates the main thread (`0x0EA59B`) and returns 1. On the console the kernel's start-up thread ends there; `qilin_run` keeps its first thread, which gives the processor up for good and wakes every 10 ms for the host's housekeeping. It is the host's first thread, the one a window's events belong to on macOS, so it is where the window will be pumped from (the title's main thread is another host thread). `QILIN_EXIT_AFTER_ENTRY=1` ends the process when the entry point returns, for the test programs.

It reads an extracted disc (a folder, or the `default.xbe` in one). Given a disc image it says to extract it first (`qilin extract`); reading the image directly in the I/O manager is a later step.

## Virtual time (`--virtual-time`, v0.10.0)
Everything in the runtime that reads time reads one clock (`host/clock.*`): the scheduler's waits, timeouts, timers and quantum, `KeTickCount`, `KeQuerySystemTime`, `KeStallExecutionProcessor`, the time-stamp counter behind `rdtsc` (which the engine calibrates against XAPI's `QueryPerformanceCounter`, itself `rdtsc`, and takes its frame time from), the display's vblanks and Swap's wait for them, DirectSound's play position, and the host's housekeeping hooks. Normally it is the host's steady clock, and a run's course depends on how fast the host is: on a software renderer a frame takes 50–70 ms, so the game's time step, and with it where a held stick leaves the player, differs from run to run.

With `--virtual-time` the clock moves only as the guest does:
- **When the processor goes idle** (every guest thread waits), the thread leaving it moves the time on to the earliest wait's end or timer and does what the clock thread does in real time (ends those waits, fires the timers, updates the tick count, runs DPCs, dispatches). There is no clock thread.
- **By work**: lifted code's preemption points (its backward branches) always call `preempt()`, which spends 50 ns a call and takes the scheduler's lock only when something is due (a wait's end, a timer, the quantum, the next millisecond's tick) or a switch was asked for; each `rdtsc` spends 20 ns. Spin loops end, quanta end, at points the guest's own code decides.
- DirectSound's mixer thread doesn't run: the mix is brought up to the clock by the game's calls and the housekeeping hook, so packet completions come at the same points. No sound device is opened (it would play in real time); `--dump-audio` still writes. The frame rate is the console's.
- `KeQuerySystemTime` starts at 2001-11-15 00:00 UTC.

The result: two runs with the same presses give the same frames, pixel for pixel (2,400 frames from power-on to the title screen, intro movie included, checked in v0.10.0). Press scripts written for a real-time run don't carry over (loading takes fewer frames in virtual time: the costs are guesses at the console's speed, not measurements), but once written they land every time. It is also what the differential test and frame-hash regression tests will need. A run is a little slower than real time on the cloud workspace's software renderer (2,400 frames in about 95 s).

## Reaching places a script can't yet (v0.14.0)
Two debugging aids for scripted runs, set in the environment and never in normal use. `QILIN_DUMP_RAM=frame,file` writes all of guest memory to a file at that frame; comparing two dumps finds where the game keeps something (a counter, a position, a flag). `QILIN_POKE=frame:addr=value[,addr=value…]` (hexadecimal, 32-bit words) writes guest memory once at that frame. With virtual time the game's heap is laid out the same way every run, so an address found once stays valid for the same presses.

This is how every trail was reached (v0.14.0) without playing the stables' tutorial: the game keeps which trails are unlocked in an array of words at `0x266000`, indexed by level (2 is Forest Trail 1 … 10 is Shoreline Trail 3; the check is at `0xCF27D`), and the stables' level triggers, read from the stables' script, as records 0x60 bytes apart from `0x18D18C0` (Forest Trail 1's; position, then the radius squared at +0x14 and the radius at +0x18). Unlocking a trail and widening its trigger at frame 5600 of the yard script brings up "Do you want to explore …?" in the yard; A loads the trail. `CLAUDE.md` has the presses.

## End-to-end tests without the game
`recomp/tests/make_test_xbe.cpp` writes small hand-assembled Xbox programs as XBE files. CMake lifts each with `qilin lift`, builds it with `run.cpp` and the runtime exactly as the game is built (`qilin_add_program`), and CTest runs it:

| Program | What it does | Passes when |
|---|---|---|
| `hello` | calls the kernel through its thunk table, loops, writes its BSS | the entry point returns 42 |
| `files` | opens `\Device\CdRom0\HELLO.TXT`, reads it, closes it | it returns the sum of the file's bytes |
| `missing` | calls a kernel function the runtime lacks | the runtime stops naming `DbgPrint`, its ordinal and the caller |

So the whole path, from an executable through the lifter and each CI compiler to a running native program over the native kernel, is exercised on every push. New kernel work gets a program here.

## Next
Whatever later scenes call that the opening, the stables and the trails haven't: a stop names it, and the bring-up loop provides it (`CLAUDE.md`).

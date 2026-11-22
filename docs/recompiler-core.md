# The recompiler core

The tools in `recomp/src/` are GBTGhoulies--QilinRecomp's (v0.41.1, the same owner), brought over whole because the console, the executable format, the instruction set and the compiler generation are the same. This file says what they are, what was changed, and what is not yet known. That project's `docs/recompiler-core.md` is the long record of how each pass came to its design and of the dead ends; read it before reworking a pass.

All of it now runs on the game: the analysis, the lift of every function lifted code reaches, the decoder cross-check and the differential test (below).

## What is here
| File | What |
|---|---|
| `xbe.*`, `xbe_json.cpp` | The XBE loader: headers, certificate, sections, library versions, kernel thunks, a flat image. Every read is bounds-checked; a malformed file is an error message |
| `xiso.*` | The Xbox DVD file system: the game partition found at its offset in a full-disc dump or at 0, the volume descriptor, the directory trees. New in this project |
| `sha256.*` | SHA-256, to recognise the supported build (from WindWakerHD--QilinRecomp) |
| `decoder.*` | Zydis wrapped: compact instructions, cached; flags read, written and defined per instruction |
| `cfg.*`, `vrange.*` | Function discovery and control flow: linear seeds, recursive descent, switch tables with proven sizes (value-range analysis), C++ exception and SEH tables, validation that drops data decoded as code, byte coverage |
| `lifter.*` | One function to C++: registers in host locals, flags only where read, labels and gotos, `switch` for jump tables, integer, scalar SSE, x87 with precision control, string operations, direct, indirect and tail calls, C++ exception frames |
| `game.hpp` | The supported build's facts. Empty |
| `main.cpp` | The `qilin` CLI |
| `../runtime/guest.hpp`, `x87_math.hpp`, `dispatch.cpp` | What lifted code compiles against: the context, memory, flag and float helpers, x87 arithmetic and transcendentals, calls through the function table |

## What was changed from the sister project
- **Everything specific to its game was taken out and given one home, `game.hpp`:** the `.text` regions, the lifted and library ranges, the functions lifted or replaced one by one, the start-up roots, the C runtime's hand-written-assembly conventions, the aligned regions, the library data section. All are empty. `CfgConfig`'s defaults no longer describe any game.
- **With nothing recorded, the tools assume only what the XBE says.** The whole of `.text` is one region, and it counts as lifted, so `qilin coverage` exits 3 until every byte in it is accounted for or the regions are drawn. No function alignment is assumed.
- **The build is identified.** `qilin info` prints the file's SHA-256. Once `game::kSha256` is set, every command refuses another executable (exit 4) unless given `--any-build`.
- **The game can be given as it is:** a disc image, an extracted folder or `default.xbe`. `qilin disc` and `qilin extract` are new, as are `qilin imports` (kernel imports by name; an ordinal the kernel doesn't export is an error) and `qilin align`.
- **The coverage test now tests something:** the sister project's unit test compared a region that didn't fit the synthetic section, so it counted no unknown bytes whatever the pass did. Here it pins the exact accounting of the synthetic `.text` (the same fix is worth making there).
- **A data-pointer seed can start inside a real function's instruction and hide it** (v0.8.x, found by a run: the mini-games called `0x800C0` through a pointer and nothing had been lifted there). A data word equal to `0x800F0` seeded a function in the middle of the instruction at `0x800EE`; the real function at `0x800C0`, known only by an immediate (`push 0x800C0` at `0x80442`), was then explored only up to that false entry, judged unclean and dropped. Now an immediate pointer is always kept as a seed (before, not if it was a function at that moment, so a function dropped later was never tried again), and when one is accepted, weakly-evidenced functions whose entries its own instructions step over are evicted first (`Cfg::evict_misaligned`, once per pointer; `bound_over_misaligned`). On this game that changes exactly one function and takes the game region's unknown bytes from 67 to 23.
- The Python analysis prototype, the differential test and the checks written against that game's C runtime addresses were left behind. The differential test came over in v0.12.0 with this game's conventions (below).

## Known heuristics that came from the other game
These are in the code as they were. Each is a property of that game's build that must be measured here, not trusted:
- **16-byte entries.** A pointer in a data section is a function-pointer candidate only if it is a multiple of 16, and gap seeding only tries 16-aligned starts (`cfg.cpp`). `qilin align` reports how this game's directly-called functions are aligned; if they aren't 16-aligned, both rules change before the function count means anything.
- **Section names.** The loader counts every section as code but `.rdata`, `.data` and the handful of data sections that game has (`kNonCode` in `xbe.cpp`). A data section this game has and that one doesn't will show in `coverage.txt` as a section full of unknown bytes.
- **Gap seeding in `.text` only** (`CfgConfig::gap_section`), and pointers scanned in `.rdata` and `.data` only.
- **"Implausible instruction" rules** (`Insn::implausible_reason`) describe what retail MSVC code of that game never contains.
- **The C runtime's conventions** (flags passed between x87 helpers, `__SEH_prolog`, shared epilogues) are found by the lifter refusing those functions; that game's list is in its `main.cpp` history as a guide to what to look for, at other addresses.

## Checks
- `ctest`: the unit tests (loader, decoder, CFG, lifter, runtime helpers, the disc reader with malformed images, SHA-256), the `qilin` tool run on the synthetic XBE, and the x87 transcendentals' fast paths against their slow paths.
- `tools/check/verify_decoder.py`: Zydis against LLVM on `qilin insns` listings. Needs the game. **Run on this game (v0.11.0, libLLVM 18):** all 500,668 reachable instructions agree in length and mnemonic; the linear sweep's 670,764 instructions show 417 disagreements, all on bytes that are data and all of known-benign classes (LLVM rejecting `bound`, `int1`, SAL /6 and the like; a lone prefix; encodings no Pentium III runs; and, new here, an F2/F3 prefix on a packed-single SSE opcode that has no form with it, which Zydis rejects and LLVM decodes without the prefix). The one far jump in reachable code (`0xEF4FD`) is in XAPI's function at `0xEF4E4`, which `qilin suspects` lists for changing the interrupt flag; no run has reached it. To run it: `qilin insns <game> -o reach.txt`, `qilin insns <game> --linear -o linear.txt`, `verify_decoder.py --reachable reach.txt --linear linear.txt` (about 4 s).
- `tools/check/x87/rounding.py`, `transcendental.py`: the x87 arithmetic against exact rational results and mpmath. No game needed.
- `tools/difftest/difftest.py`: every lifted function against the original x86 under Unicorn (below). Needs the game.

## Differential test (`tools/difftest/`, v0.12.0)
The sister project's, brought over with this build's addresses; its `docs/recompiler-core.md` describes the method in full (random guest states, the original's whole call tree in Unicorn, the lifted code on an identical copy, registers, x87 state, xmm lanes and every written byte compared bit for bit, the emulator corrections, the runaway guards). It lifts everything into a shared library built at `-O2` with warnings as errors, bounds-checked and write-traced memory. It needs Python's `unicorn` (2.1) and `mpmath`, a C++23 compiler and the game:

```
difftest.py --qilin build/qilin --xbe default.xbe --work /tmp/qd --cxx g++-14 --trials 8
```

About 35 minutes for every function on the cloud workspace's two cores; `--test ADDR,...` runs some, `--reuse` keeps the built library.

**This build's settings** (the top of `difftest.py`): the code pages (`0x11000`–`0x20C000`), the C runtime's range for its x87 checks, the shared epilogues that pop a caller's word (`GLUE_POPS`: `_NLG_Notify`'s body, `strchr`'s exit, the x87 maths' epilogues), the 17 C runtime helpers that take their operands on the x87 stack (`X87_ARGS`, found by walking each one's fall-through path, checked by reading them), and the helper that can return to its caller's caller (`RETURNS_PAST`).

**Result (v0.12.0): 6,772 functions, 40,632 trials, 0 mismatches.** 4,264 functions completed at least one matching trial; in the rest every trial faulted in both versions alike (random pointers) or ran past the instruction budget. The first run had 23 mismatched functions. One was a lifter bug, the rest were the test:
- **Lifter: exp's overflow path trapped.** The x87 maths' range check before `f2xm1` (`0x148D06`) jumps, out of range, to glue (`0x148CB8`) that drops its own return address and runs the shared error path in its caller's frame, returning to the caller's caller. Lifted, the caller (`exp`, `0x152278`) found its callee back somewhere other than the call site and trapped. `Conventions::returns_past_caller` now names such helpers; a call to one accepts that return when esp is where the caller's own `ret` would leave it, and the caller returns too. The other caller (`0x148BD6`) is in an orphan function that isn't lifted.
- **`rdtsc`** (7 functions): the harness trapped on it, where the reference has a clock that restarts every trial. The harness now has the same clock.
- **`fprem` with operands far apart in exponent** (`0x147830`, the C runtime's `fmod` loop): Unicorn's remainder is wrong (1 mod 3.3e-38 gave 0); the lifted code's was exact. Both emulators now get the exact remainder and quotient bits, a complete reduction, which is the lifted code's specification.
- **`fyl2x` after `fldln2`** (`0x16D8C0`, `log`): the reference computed with the original's 64-bit ln 2, the lifted code holds the double. The reference now rounds the operands to double first, which is what the lifted registers hold, so it checks the specification.
- **Random heap doubles near 1e-154** (`0x23420`, a vector normalise): their squares leave double's exponent range, which the original's 15-bit exponent keeps. The check the C runtime already had (a register outside double's range after an x87 instruction skips the trial) now also covers the function under test. Real game data never comes near.
- **The x87 operand helpers** (3 functions) were run with an empty stack half the time: now listed in `X87_ARGS`. **NaN against NaN** in an x87 register (`fprem` of infinities) counts as `pass-nan`, as in xmm lanes.
- **Entries that run in another function's frame** (10): the secondary entry points and the parent's code past a `__finally` block that the CFG split off (`call __SEH_epilog; ret` tails), which return through a frame a trial can't build. `qilin lift` now writes them out (`gen/secondary.txt`, `gen/tail_only.txt`); a stack imbalance there counts as `fragment`. And a trial where both versions overran a buffer onto their return address counts as faulting in both once the original's first fetch outside the code is that address (a hook on non-code addresses records it; before, the original ran on through mapped data and the two runs looked different).

Unchecked: an ARM64 host, a UBSan build (`--sanitize`), and mutation checks on this build (the sister's showed planted lifter bugs are caught).

## Next
In the order of `roadmap-and-design-targets.md`: the first analysis; `game.hpp` filled in; the decoder cross-check; coverage to zero; every function lifting; the differential test.

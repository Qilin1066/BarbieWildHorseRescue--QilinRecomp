# The recompiler core

The tools in `recomp/src/` are GBTGhoulies--QilinRecomp's (v0.41.1, the same owner), brought over whole because the console, the executable format, the instruction set and the compiler generation are the same. This file says what they are, what was changed, and what is not yet known. That project's `docs/recompiler-core.md` is the long record of how each pass came to its design and of the dead ends; read it before reworking a pass.

**None of this has been run on the game.** Every result so far is on the synthetic XBE and disc image the tests build in memory.

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
- The Python analysis prototype, the differential test and the checks written against that game's C runtime addresses were left behind. The differential test comes over in step 2 with this game's conventions.

## Known heuristics that came from the other game
These are in the code as they were. Each is a property of that game's build that must be measured here, not trusted:
- **16-byte entries.** A pointer in a data section is a function-pointer candidate only if it is a multiple of 16, and gap seeding only tries 16-aligned starts (`cfg.cpp`). `qilin align` reports how this game's directly-called functions are aligned; if they aren't 16-aligned, both rules change before the function count means anything.
- **Section names.** The loader counts every section as code but `.rdata`, `.data` and the handful of data sections that game has (`kNonCode` in `xbe.cpp`). A data section this game has and that one doesn't will show in `coverage.txt` as a section full of unknown bytes.
- **Gap seeding in `.text` only** (`CfgConfig::gap_section`), and pointers scanned in `.rdata` and `.data` only.
- **"Implausible instruction" rules** (`Insn::implausible_reason`) describe what retail MSVC code of that game never contains.
- **The C runtime's conventions** (flags passed between x87 helpers, `__SEH_prolog`, shared epilogues) are found by the lifter refusing those functions; that game's list is in its `main.cpp` history as a guide to what to look for, at other addresses.

## Checks
- `ctest`: the unit tests (loader, decoder, CFG, lifter, runtime helpers, the disc reader with malformed images, SHA-256), the `qilin` tool run on the synthetic XBE, and the x87 transcendentals' fast paths against their slow paths.
- `tools/check/verify_decoder.py`: Zydis against LLVM on `qilin insns` listings. Needs the game.
- `tools/check/x87/rounding.py`, `transcendental.py`: the x87 arithmetic against exact rational results and mpmath. No game needed.

## Next
In the order of `roadmap-and-design-targets.md`: the first analysis; `game.hpp` filled in; the decoder cross-check; coverage to zero; every function lifting; the differential test.

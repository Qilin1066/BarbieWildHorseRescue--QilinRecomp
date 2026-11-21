# Code analysis: the regions of `.text`, what is lifted, what is native

Results on the supported build (`docs/xbe-analysis.md`), v0.5.0. Addresses are in the XBE as loaded.

## The regions
`.text` runs from `0x011000` to `0x167098`. `recomp/src/build.hpp` (`game::kTextRegions`) divides it in five:

| Region | Range | Lifted | Evidence |
|---|---|---|---|
| `game` | `0x011000`–`0x0E9BDC` | whole | Every directly-called function starts on a 16-byte boundary (1,882 of them). Its assertions name the game's source files (`.\Trail\…`, `.\MainMenu\…`, `.\Ranch\…`, `.\SubGames\…`, `.\Main\Game.cpp`). The last aligned function is at `0x0E9B60`; `0x0E9BDC` is the first function off the grid |
| `xapi` | `0x0E9BDC`–`0x0EFC00` | function by function | Unaligned entries, the XBE's entry point (`0x0EA60F`), kernel calls through the thunk table, six-byte import stubs at its end (`0x0EFBCE`–`0x0EFBF2`) |
| `babel` | `0x0EFC00`–`0x146506` | whole | 16-byte entries again (685 called functions, all aligned). Blitz Games' engine: its strings call its heap the "Babel heap", its functions `bk…`, `bd…`, `ba…`, `b…` (`bdOpenDisplay`, `bkGenerateEvent`, `baGenerateShader`, `bPlaySample3D`), its source files `.\xbHeap.cpp`, `.\xbRender.cpp`, `.\xbAudioStreamWMA.cpp`, `.\package.cpp`, `.\xbShaderSelector.cpp`. The Xbox layer of a multi-platform engine: heap, rendering over Direct3D, input, files and packages, a file cache on the `Z:` partition, background loading, sound over DirectSound, WMA and ADPCM streams, XMV playback, game saves |
| `libs` | `0x146506`–`0x160130` | function by function | Unaligned from `0x146506` on: the C runtime (its buffer-overrun report, `printf`'s `(null)`, `1#SNAN`, the x87 maths helpers) and more of XAPI (saved games' `SaveMeta.xbx`, the soundtrack paths under `TDATA\FFFE0000\MUSIC`). The two are not yet told apart |
| `eh-funclets` | `0x160130`–`0x167098` | whole | Nothing but exception-handling funclets and handler stubs (8 to 25 bytes each) from `0x160130`, the end of the last function longer than 40 bytes; they refer to the game's and the engine's objects |

The layout is the sister game's: the title's own code, XAPI, the title's libraries, the C runtime, the funclets.

## Accounting (`qilin coverage`)
| Region | Code | Tables | Padding | Unknown | Accounted |
|---|---|---|---|---|---|
| `game` | 858,870 | 5,657 | 23,178 | **67** | 99.992% |
| `babel` | 345,353 | 2,194 | 7,019 | **0** | 100% |
| `eh-funclets` | 25,701 | 0 | 2,819 | **0** | 100% |
| `xapi` | 24,579 | 0 | 24 | 9 | (lifted by reach) |
| `libs` | 77,416 | 235 | 3,679 | 24,184 | (lifted by reach: the unknown bytes are library functions nothing reaches) |

The 67 unknown bytes in `game`:
- `0x0800C0`–`0x0800EF` (44 bytes): a function nothing refers to, which gap seeding doesn't accept (to look at: probably one that ends in a call that doesn't return).
- Three places of 7 to 8 bytes at `0x0567B3`, `0x0567EE` and `0x05741A`: the tails of byte index tables whose length isn't proven.

Still open in the control flow: **73 indirect jumps unresolved** and 9 jump tables sized by heuristic. Every table in lifted code must have a proven size before the lifted code is trusted there.

Function discovery changed in two ways for this game (both general): a `mov` or `push` of a segment register marks data decoded as code (a false seed at `0x07A120`, inside a `push` of a real function, was kept alive by one); and gaps are seeded again after validation drops a false seed, which had been hiding the real function at `0x07A010`.

## What is lifted
`qilin lift`: **6,621 functions, all of them lift** (v0.5.0). 5,250 are the three whole regions; 1,371 are library functions lifted code can reach (XAPI, the C runtime, XGraphics, the WMA decoder), followed from the entry point. 96 call targets have no lifted body and get stubs that stop the runtime, named: the hardware libraries' functions.

The lifter gained, for this game: the one-operand 8-bit multiply (`imul r/m8`, `mul r/m8`), `xlat`, `sfence` (nothing to do), `fstp m80` and `fprem` (the C runtime's `fmod` and number formatting).

### The C runtime's conventions (`game::conventions()`)
The same hand-written assembly as the sister game's C runtime, at other addresses, each read before it was listed:

| Address | What | Exception |
|---|---|---|
| `0x149015`, `0x149058` | x87 maths: load the operand, return ZF if it is infinite or a NaN | ZF out |
| `0x1474FD`, `0x147605` | the routines called next, which branch on that ZF first | ZF in |
| `0x1478A4`, `0x1478DF` | `__SEH_prolog`, `__SEH_epilog` | build and tear down the caller's frame |
| `0x151DD4` | `__EH_prolog` | returns through a pushed address with the frame built |
| `0x146799` | `pop eax; pop ecx; xchg [esp], eax; jmp eax` | calls a `__finally` block |
| `0x146C24` | `_NLG_Notify`'s shared body, entered by a jump after its two pushes | pops what its other entry pushed |
| `0x146D50` | `strchr`'s shared exit | pops its caller's `ebx` |
| `0x148CB8` | x87 maths: `pop eax` drops its return address | returns to its caller's caller |
| `0x14906E`, `0x14907B`, `0x1490B9` | x87 maths epilogues | pop the saved control word, return |

### Native (`game::kNative`, `recomp/runtime/host/game_native.cpp`)
| Address | What | The runtime |
|---|---|---|
| `0x0EBDEC` | XAPI start-up: patches a word of kernel code on the kernel builds it knows (`cli` … `sti`), then through `0x0EBD88` reads the kernel image's headers and swaps two descriptor-table words | does nothing: there is no kernel image and no descriptor table |
| `0x0EF4E4` | the descriptor-table swap itself (`sgdt`, `cli`, `xchg`, a far jump) | not registered: reached only through the above; stops, named, if anything else calls it |
| `0x150018` | `OutputDebugStringA` (`int 0x2D`) | prints the string |
| `0x115580` | the engine's `wbinvd; ret` | does nothing |

`rdtsc` (XAPI's `QueryPerformanceCounter` at `0x14FFF7`, the engine's timers at `0x0F6026`–`0x0F60F4`) is lifted and reads the runtime's 733 MHz counter.

## The first run
The lifted game compiles (34 generated files; about six minutes with GCC 14 at `-O2` on two cores, no warnings from the generated code's own build) and starts:

```
qilin_run: 64 MB of guest RAM, 83 replacements registered; calling the entry point at 000EA60F
qilin_run: thread 2 started (000EA59B, priority 8)
qilin_run: the entry point returned 1
qilin: stopped at 002040BD: no replacement for XPP code at 002040BD, called from 000EBD55
```

XAPI's entry point, lifted, creates the title's main thread through the runtime's kernel and returns; the main thread runs XAPI's start-up as far as its first call into a library that drives hardware (XPP, the controller library), where it stops by name. That is the boundary the next work starts from: the 96 stubs are the list.

The differential test (lifted code against the original x86) has not run yet, so "lifts" and "runs this far" are not yet "computes the same".

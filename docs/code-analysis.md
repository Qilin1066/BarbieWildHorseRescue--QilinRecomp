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

## The library functions by name
`tools/gen/xdk_symbols.py` ran XbSymbolDatabase (MIT, commit `20eced5`) on the executable: **322 symbols** of XDK 5558 named (`recomp/runtime/host/xdk_symbols.inc`): 151 in Direct3D, 102 in DirectSound, 62 in XAPI and the controller library, 6 in XGraphics. None in XMV.

### What lifted code calls in the replaced libraries (the stubs)
96 call targets have no lifted body. They are the work list for the hardware libraries:

| Library | Count | Functions |
|---|---|---|
| Direct3D | 48 | `Direct3D_CreateDevice`, `D3DDevice_Swap`, `_Clear`, `_DrawVertices`, `_DrawVerticesUP`, `_SetVertexShader`, `_CreateVertexShader`, `_SetVertexShaderConstant1`/`4`/`NotInline`, `_SetShaderConstantMode`, `_SetStreamSource`, `_SetTexture`, `_SetPalette`, `_SetTransform`, `_SetMaterial`, `_SetLight`, `_LightEnable`, `_SetViewport`, `_SetRenderTarget`, `_GetBackBuffer2`, `_GetDepthStencilSurface2`, `_SetRenderState_Simple`/`FogColor`/`CullMode`/`TextureFactor`, `_SetTextureState_TexCoordIndex`, `_CreateTexture2`, `_GetDeviceCaps`, `_KickPushBuffer`, `_SetVerticalBlankCallback`, `_PersistDisplay`, `_Release`, the overlay calls (`_EnableOverlay`, `_UpdateOverlay`, `_GetOverlayUpdateStatus`: the movie player's), `D3DResource_Release`/`IsBusy`/`Register`/`BlockUntilNotBusy`, `D3DVertexBuffer_Lock2`, `D3DPalette_Lock2`, `D3DTexture_GetSurfaceLevel2`/`LockRect`, `D3D_GetDeviceCaps`, and three unnamed (`0x167250`, `0x174900`, `0x1749F0`) |
| DirectSound | 27 | `DirectSoundCreate`, `DirectSoundDoWork`, `DirectSoundUseFullHRTF`, `IDirectSound_CreateSoundBuffer`/`GetCaps`/`GetSpeakerConfig`/`DownloadEffectsImage`/`CommitDeferredSettings`/`SetAllParameters`/`SetI3DL2Listener`/`Release`, `IDirectSoundBuffer_Play`/`Stop`/`GetStatus`/`GetCurrentPosition`/`SetBufferData`/`SetNotificationPositions`/`SetVolume`/`SetFrequency`/`SetPosition`/`SetVelocity`/`SetMinDistance`/`SetMaxDistance`/`SetRolloffCurve`/`Release`, `IDirectSoundStream_SetVolume`, and one unnamed (`0x1A5BC3`) |
| XMV | 6 | unnamed: `0x1DAE6D`, `0x1DB3B1`, `0x1DB495`, `0x1DB502`, `0x1DB681`, `0x1DB6A0` |
| Controller library | 9 | `XInitDevices` (through its jump at `0x20421F`), `XGetDevices`, `XGetDeviceChanges`, `XInputOpen`/`Close`/`GetState`/`SetState`: **provided since v0.6.0**; `XMountMUA`, `XUnmountMU`: an empty slot's answer (v0.11.0) |
| In `.text` | 6 | the four natives above (`OutputDebugStringA` among them), and `0x14F143` and `0x15FF41`, labels inside library functions that the discovery pass took for tail calls (v0.11.0: see below) |

The game draws through a small part of Direct3D: no index buffers, no `DrawIndexedVertices`, vertex shaders throughout (the engine generates them: `baGenerateShader`), palettes, and the overlay for movies.

### Static initializers of replaced libraries
The C runtime's start-up runs three tables of initializers (`0x231860`, `0x231870`, `0x2318AC`). Seven entries belong to libraries that are replaced, and each only stores into its library's own data: three of the controller library's (`0x2040BD`, `0x20B291`, `0x20B2C1`) and four of DirectSound's (`0x1A7153`, `0x1A715E`, `0x1AA01C`, `0x1AA027`). They are lifted (`game::kLiftedLibraryFunctions`), so start-up runs its tables as on the console.

## The run so far
The lifted game compiles (34 generated files; about six minutes with GCC 14 at `-O2` on two cores) and starts. With v0.7.0:

```
qilin_run: thread 2 started (000EA59B, priority 8)
qilin_run: the entry point returned 1
qilin_run: thread 3 started (00118460, priority 8)
qilin_run: DirectSound: 48000 Hz software mix on its own thread, to stereo
qilin_run: DirectSound: effects image of 24824 bytes (I3DL2 reverb at effect 0, crosstalk at 1), run natively
qilin: stopped at 00174900: no replacement for D3D code at 00174900, called from 000F1995
```

XAPI's entry point creates the title's main thread and returns. The main thread runs XAPI's and the C runtime's start-up (initializer tables, heap, time zone, drive letters), enters the game's `main` and the engine's start-up: its heap, its background loader's thread (thread 3), its input (the controller library, natively), a write test on the disc that fails as it should, its sound start-up (`bInitSpecificWave`: DirectSound created, the effects image downloaded, the listener set), and then its display start-up (`0x0F1970`), whose first Direct3D call stops it. That call, `0x174900`, is `mov eax, 1; ret 4`: the library's object creation (it returns the constant the XDK uses for the one Direct3D object), a pure function to lift.

What each stop needed, in order: the replaced libraries' static initializers lifted; the settings' whole user section (`ExQueryNonVolatileSetting` index `0xFF`, read by `GetTimeZoneInformation`); `RtlTimeToTimeFields`; named events and mutants; the controller library; the Win32 error for a write-protected volume; DirectSound.

The differential test (lifted code against the original x86, v0.12.0) passes over every lifted function: "lifts" is now also "computes the same" on random states (`recompiler-core.md`, "Differential test").

## What the discovery pass leaves dynamic (v0.11.0)

Every byte of the lifted regions is now accounted for (code, tables or padding): the last 23 unknown bytes were the byte index tables of three switches whose index is in `ebp` (`0x5632C`, `0x564DB`, `0x5707B`). An `ebp`-based operand is SS-relative by default, and jump-table recognition took only DS; on the Xbox every segment is flat. Two of the C runtime's switches compare against a constant pushed and popped into a register (`push 7; pop ecx; cmp eax, ecx`, its formatted output's state machine), which the value-range analysis doesn't follow through the stack; recognised now. Heuristic jump tables: 9 → 4, every target as before (1,837). The four left are Direct3D's switches on a texture format byte (`0x16DA3F`, `0x16DB5B`, `0x16E6D6`, `0x1733C7`), which have no bound in the code at all.

The 73 unresolved indirect jumps are all run-time targets by nature, which lifted code sends through the dispatcher: 51 through a pointer in an object (C++ virtual calls made as tail calls), 17 through a global pointer (import thunks, a handler table in `.data` at `0x24D2A8` indexed by a type byte), 3 to a register (the C runtime's exception handling, to catch-block continuations), and a far jump in XAPI's shutdown (`0xEF4FD`).

Two jumps in library code end in stubs (`call_unlifted`, a stop if reached): `0x14F07B` → `0x14F143`, in the C runtime's file-handle allocator (`0x14F039`, called by `_open`), where a `__finally` block between the function's parts made the pass split it; the branch allocates a second block of handles, so it would take some thirty C runtime files open at once, and the engine does its file work through XAPI. And `0x160004` → `0x15FF41`, in an SSE routine nothing references: a data word equal to `0x00160000` made the library closure lift a piece of it as a function. Merging such pieces into their parent (a piece that jumps back into the middle of the function before it; a handler the function falls into) was tried and destabilised the pass (merges went from 434 to over 800, with new false functions), so both are left as they are and named here.

# default.xbe: first analysis

Source: the owner's `Barbie Horse Adventures - Wild Horse Rescue (USA)` dump, as an extracted disc; `default.xbe` is 2,453,504 bytes. Produced with `qilin info`, `imports`, `cfg`, `align`, `coverage`, `suspects` and `lift` (v0.4.0). The disc image itself has not been read yet (it is too large to bring to a cloud session), so `qilin disc` and `qilin extract` are still untried on a real image.

## Identity
- **Type:** retail XBE (the entry point and kernel thunk table decode with the retail keys).
- **Title:** "Barbie(tm) Horse", title ID `5655002A` (VU-042), region flags `3`, version 5, disc 0, allowed media `2`.
- **SHA-256:** `51e6aa2a064be35414866d1f048b184f174db749a3c65156211d5307d2bda7e4` (recorded in `recomp/src/build.hpp`: the tools and `qilin_run` refuse any other executable).
- **Build timestamp:** 2003-09-26 16:43:03 UTC.
- **Debug path left in the header:** `c:\BarbieHorse\XboxReleaseCD\BarbieHorse_Xbox_R_CD.exe`.

## Memory layout
- Base `0x00010000`, image size 2,982,208 bytes.
- Entry point `0x000EA60F` (in `.text`, in the unaligned stretch that is XAPI: see below).
- Kernel thunk table `0x0020B340` (the start of `.rdata`), TLS directory `0x002235B4`.
- Stack commit 64 KB; heap reserve 1 MB, commit 4 KB; init flags `0x00000005`.

## Sections
| Section | Address | Virtual size | In the file | What it is | Plan |
|---|---|---|---|---|---|
| `.text` | `0x011000` | 1,400,984 | all | The game, its engine, XAPI and the C runtime | Lift |
| `D3D` | `0x1670A0` | 78,984 | 64,464 | Direct3D 8 | **Replace** over the Vulkan renderer; its pure resource code lifted, as in the sister project |
| `XGRPH` | `0x17A540` | 171,976 | 168,780 | XGraphics | Lift what lifted code reaches |
| `DSOUND` | `0x1A4520` | 120,228 | 119,612 | DirectSound | **Replace** over the mixer |
| `WMADEC` | `0x1C1AE0` | 103,028 | all | The WMA decoder | Lift |
| `XMV` | `0x1DAD60` | 163,292 | 163,276 | The XMV video player | To decide: lift its decoder over the replaced Direct3D and DirectSound, or replace it |
| `XPP` | `0x202B40` | 34,804 | all | Peripherals (controllers, memory units) | **Replace** over SDL input |
| `.rdata` | `0x20B340` | 156,948 | 156,932 | Constants, the kernel thunks | Data |
| `.data` | `0x231860` | 706,364 | 157,408 | Globals; the rest is BSS | Data |
| `DOLBY` | `0x2DDFA0` | 29,056 | 29,036 | Dolby encoder microcode for the audio processor | Not run |
| `.data1` | `0x2E5120` | 224 | 176 | Data | Data |
| `$$XTIMAGE` | `0x2E5200` | 10,240 | all | The title image (not preloaded) | Data, loaded on demand |
| `sig` | `0x2E7A00` | 336 | all | Data (not preloaded) | Data |
| `.XTLID` | `0x2E7B60` | 1,496 | all | Library identification | Data |

A section's flags don't tell code from data here (`.rdata` is marked executable), so the loader goes by name; `.data1` and `sig` were added to its list of data sections in v0.4.0.

## XDK libraries
All from **XDK 5558**, QFE 1, approved level 2, none debug: `XAPILIB`, `D3D8`, `XGRAPHC`, `XBOXKRNL`, `LIBCMT`, `DSOUND`, `XMV`, `XVOICE`.

Compared with the sister game (XDK 5659): no XACT (the game drives DirectSound itself), no Bink (its movies are XMV), no D3DX section, and XVOICE is listed. XbSymbolDatabase's coverage of 5558 is to check when the library functions are named.

## Kernel imports
120. The runtime provides 80 kernel exports, of which this game imports most; **36 imported functions have no native code yet:**

`AvGetSavedDataAddress`, `AvSendTVEncoderOption`, `AvSetDisplayMode`, `AvSetSavedDataAddress`, `DbgBreakPoint`, `DbgPrint`, `HalGetInterruptVector`, `HalInitiateShutdown`, `HalIsResetOrShutdownPending`, `HalReadWritePCISpace`, `HalReturnToFirmware`, `IoCreateDevice`, `IoDismountVolumeByName`, `IoInvalidDeviceRequest`, `IoMarkIrpMustComplete`, `IoStartNextPacket`, `IoStartPacket`, `IofCompleteRequest`, `KeConnectInterrupt`, `KeDisconnectInterrupt`, `KeInitializeInterrupt`, `KeSynchronizeExecution`, `MmClaimGpuInstanceMemory`, `MmGetPhysicalAddress`, `MmLockUnlockBufferPages`, `MmLockUnlockPhysicalPage`, `MmQueryAddressProtect`, `MmQueryAllocationSize`, `MmSetAddressProtect`, `NtClearEvent`, `NtQueueApcThread`, `NtUserIoApcDispatcher`, `RtlTimeFieldsToTime`, `RtlTimeToTimeFields`, `RtlUnwind`, `XcVerifyPKCS1Signature`.

Most of these (the `Av`, interrupt, PCI, `Io` device and page-locking ones) are what the hardware libraries call, and go away with them when Direct3D, DirectSound and the controller library are replaced. Which the lifted code itself reaches is found at the first run, where each stop names one.

Imported variables (13): `ExEventObjectType`, `HalBootSMCVideoMode`, `HalDiskCachePartitionCount`, `IdexChannelObject`, `KeTickCount`, `LaunchDataPage`, `XboxAlternateSignatureKeys`, `XboxHDKey`, `XboxHardwareInfo`, `XboxKrnlVersion`, `XboxSignatureKey`, `XeImageFileName`, `XePublicKeyData`. The runtime fills in two of them today (`XeImageFileName`, `HalDiskCachePartitionCount`); the others are zeroed objects until something reads them.

## Functions and control flow (first pass, nothing tuned)
- **7,659 functions**, 499,361 instructions, found in under 4 seconds: 6,795 reachable from the entry point and call targets, 481 from C++ exception tables, 229 orphans, 118 from immediates, 24 in gaps, 12 from pointers in data.
- 262 jump tables (1,837 targets); **73 indirect jumps unresolved**; 5,111 indirect call sites. (v0.11.0: the 73 are all run-time targets by nature, and 4 heuristic tables are left: `code-analysis.md`.)
- C++ exception handling is used heavily: 375 `FuncInfo` tables, 324 handler stubs, 953 unwind funclets, 2 catch entry points. SEH: 48 scope tables, 19 filter/handler pairs.
- No function with a bad decode or overlapping instructions; 66 candidates rejected as data decoded as code.
- **Byte accounting of `.text`: 97.958%.** 1,327,123 bytes of code, 8,086 of tables, 37,168 of padding, **28,607 unknown** (the target is 0).
- **Suspects (8):** interrupt-flag changes at `0xEBE8B`, `0xEF4F5` and (XPP) `0x209615`; a privileged instruction at `0x115580`; an interrupt instruction at `0x150043`; privileged code and port I/O in D3D (`0x16D0C4`, `0x16EC7D`, `0x16FB90`). The ones in `.text` are candidates for `game::kNative`.

## Function alignment
Directly-called functions in `.text` by address, 16-byte aligned or not (`qilin align` and the CFG):

| Range (to the nearest page) | Called functions | 16-aligned | Reading |
|---|---|---|---|
| `0x011000`–`0x0E9000` | 1,882 | all | The game and its engine |
| `0x0E9000`–`0x0F0000` | 158 | 28 | Unaligned: XAPI (the entry point is here) |
| `0x0F0000`–`0x146000` | 685 | all | Aligned again: more of the game, or its libraries |
| `0x146000`–end | 342 | 34 | Unaligned: the C runtime; the exception-handling funclets close the section (from about `0x166829`) |

So the build is laid out like the sister game's (aligned game code, unaligned Microsoft libraries, funclets last), and the 16-byte rule the function discovery inherited holds where it is applied. The exact boundaries, each with its evidence, are the next step (`code-analysis.md`, `game::kTextRegions`); until then `.text` is treated as one region.

## Lifting (first pass)
`qilin lift` over the whole of `.text`: **5,844 of 5,899 functions lift**, in about 4 seconds. The 55 that don't, by reason:

| Count | Reason | Likely home |
|---|---|---|
| 31 | a `pop` that takes the function's own return address | C runtime and exception-handling helpers (`__SEH_prolog`-style glue): `game::conventions()` |
| 9 | MMX and SSE integer instructions (`sfence`, `movntq`, `pminsw`, `pmaxsw`, `pinsrw`, `pcmpeqw`) | fast memory copies, or the video decoder's helpers |
| 4 | flags passed between routines | the C runtime's x87 helpers |
| 3 | `cli`, `wbinvd` | XAPI start-up: native |
| 2 each | `xlat`; one-operand 8-bit `imul`; an 80-bit x87 store | lifter work |
| 1 each | `fprem`; `int 0x2D` (the debugger's print); `mov` from `cs`; an unbalanced `ret` | lifter work, or native |

None of the lifted code has been compiled or run yet, and no differential test has run.

## The disc
An extracted disc holds `default.xbe`, a `Packages` folder and a `Video` folder.
- **Assets:** `.xbp` packages, by area (`Front`, `Graphics`, `Multi`, `Stables`, `Trail01`…`Trail09`, `Trans`, `Voice`, and `Music.xbp` at 49 MB). The format is the engine's own: unknown, and the game's code reads it, so the runtime need not.
- **Movies:** three `.xmv` files (`attr_n`, `bintro_n`, `sintro_n`).
- **Languages:** string tables `Stbl_d`, `_e`, `_f`, `_it`, `_nl`, `_uk`, `_us` (German, Spanish, French, Italian, Dutch, UK and US English); voices in a `US` folder. Dutch and UK English are not console language settings, so how the game chooses a table is to confirm. No Japanese: the settings menu lists English, German, French, Spanish and Italian.
- The executable carries its source files' names in its strings (assertions: `.\Main\Game.cpp`, `.\Graphics\Camera.cpp`, `.\AddOns\Effects\CWater.cpp` …), which will help name regions and functions.

# default.xbe: first analysis

**Not done yet.** Nothing in this project has read the game: no copy of it has been available to a session. This file is where the results go, in the form of the sister project's `docs/xbe-analysis.md`.

## How to produce it
On the owner's PC, double-click `Analyse game.bat` and choose the disc image (or `default.xbe`). It builds `qilin` and writes to `analysis-out\` (ignored by git):

| File | From | What it answers |
|---|---|---|
| `disc.txt` | `qilin disc` | The files on the disc: asset formats, movies, sound banks, more than one executable |
| `extracted\default.xbe` | `qilin extract --only default.xbe` | The executable, for the later steps |
| `info.txt` | `qilin info` | Title ID, region, build date, the debug path, sections, XDK library versions, the SHA-256 |
| `imports.txt` | `qilin imports` | The kernel functions the runtime must provide |
| `cfg.txt`, `suspects.txt` | `qilin cfg`, `qilin suspects` | Functions, jump tables, exception tables, anything that needs review |
| `align.txt` | `qilin align` | Whether function entries are aligned, per section (what `game.hpp`'s aligned regions are drawn from) |
| `coverage.txt` | `qilin coverage --list` | Every byte of the code: code, table, padding or unknown |
| `reach.insns.txt`, `linear.insns.txt` | `qilin insns` | For the decoder cross-check against LLVM (`tools/check/verify_decoder.py`) |

A session then reads those (they are small enough to stage) and fills in the sections below and `recomp/src/game.hpp`.

## To record
- **Identity:** retail or debug, title name and ID, region, version, build timestamp, debug path, the SHA-256 (into `game::kSha256`).
- **Memory layout:** base, image size, entry point, kernel thunk table, TLS directory, stack and heap sizes.
- **Sections,** each with its plan: lift, replace natively, or data. Any section the loader counts as code that isn't (the loader takes every section as code but the data sections it knows by name: a new data section shows as a wall of undecodable bytes in `coverage.txt`, and is added to `kNonCode` in `xbe.cpp`).
- **XDK libraries and versions:** which XDK build (XbSymbolDatabase coverage depends on it), which of D3D8, D3DX, XGraphics, DirectSound, XACT, WMADEC, XPP (controllers), XONLINE, Bink or another video player are linked in.
- **Kernel imports** by name.
- **The compiler:** function alignment, LTCG register conventions, C++ exception tables, SEH.
- **`.text` by region:** the game, the engine, XAPI, the C runtime, in `game::kTextRegions`, each boundary with its evidence in `code-analysis.md`.

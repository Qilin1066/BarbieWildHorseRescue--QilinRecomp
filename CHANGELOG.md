# Changelog

All notable changes to this project are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/). While the recompiler is being built, a minor bump (0.**x**) marks a milestone and a patch bump (0.x.**y**) a fix. 1.0.0 will be the first build that plays the game. Past entries are never rewritten.

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

[0.1.0]: https://github.com/Qilin1066/Barbie-Wild-Horse-Rescue---QilinRecomp/commits/main

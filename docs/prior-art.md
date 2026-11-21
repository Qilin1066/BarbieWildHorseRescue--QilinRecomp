# Prior art

## The sister project on the same console
**GBTGhoulies--QilinRecomp** (the same owner, the same licence) is a static recompilation of another original Xbox game and is this project's model in everything: requirements, architecture, tools, tests, the runtime's shape, the settings menu. Its `docs/prior-art.md` reviews the other original-Xbox recompilation efforts (xboxrecomp, XboxXBERecomp, a Halo 2 matching decompilation, XbSymbolDatabase) and records the decisions drawn from them; they hold here unchanged and are not repeated:

1. Zydis as the decoder, vendored, cross-checked against LLVM on every instruction.
2. XDK library functions named with XbSymbolDatabase (MIT) signatures; our own only where it lacks the game's XDK build.
3. Guest registers in host locals inside each lifted function, a context struct at call boundaries.
4. An unknown instruction or a decode failure is a build error; an unknown indirect target at run time is a trap with the address logged.
5. The memory aliases the game uses are found during bring-up, and anything else traps in the checked build.
6. x87 precision control honoured, transcendentals from a bundled implementation with the same bits on every host.
7. The main loop's timing analysed early (fixed or variable step decides how high refresh rates are done).
8. xemu running the original game is ground truth.

What is taken from that project is code and method, never facts about its game: every address, region, count and convention in it describes Grabbed by the Ghoulies' executable. Here they are all measured again (`recomp/src/game.hpp` starts empty).

## This game
- **Barbie Horse Adventures: Wild Horse Rescue**, developed by Blitz Games, published by Vivendi Universal Games, released for the Xbox and the PlayStation 2 in North America in November 2003 (public descriptions; nothing below has been checked against the executable yet).
- One search (2026-11-21) found no recompilation, decompilation or documented file formats for it. If one turns up, record it here with its licence before reading any of its code.

## References
- **xemu** (run, never read into this code) for behaviour and pictures.
- **nxdk**'s kernel export list (CC0), vendored.
- The Xbox DVD file system as publicly described (the volume descriptor, the directory tree); `recomp/src/xiso.cpp` is written from that description, with no code taken from an extraction tool.

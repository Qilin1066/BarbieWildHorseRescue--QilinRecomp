# Naming

**Qilin** is the author's name. It goes on the *tools*, the things that do the recompiling. It doesn't go on the game. The rules are the sister projects', unchanged.

| Thing | Name | Why |
|---|---|---|
| The repo / project | BarbieWildHorseRescue--QilinRecomp (github.com/Qilin1066/BarbieWildHorseRescue--QilinRecomp) | The game plus the toolchain that ports it, as its sister projects GBTGhoulies--QilinRecomp, ApbArcade--QilinRecomp, RhythmHeavenFever--QilinRecomp, PunchOutWii--QilinRecomp, WindWakerHD--QilinRecomp and ToyCommander--QilinRecomp |
| The recompiler CLI | `qilin` | The author's tool: short, unique, easy to type |
| C++ namespaces of the recompiler and runtime | `qilin`, `qilin::rt`, `qilin::gen`; `qilin::game` for the supported build's facts | Project namespace; avoids collisions with game, Zydis or system names |
| Build options, macros, CMake targets, environment variables | `QILIN_*`, `qilin_*` | Same prefix as the namespace, so it's obvious what owns them |
| The game executable players run | `wildhorserescue` | Named after the game. Players launch Wild Horse Rescue, not a tool |
| The window's title, SDL's app name | Barbie Horse Adventures: Wild Horse Rescue | Same reason |
| The game's data folder | `<per-user data>/qilin/wildhorserescue` | As the other recomps keep theirs (`qilin/<game>`): saves, the pipeline cache |
| The settings file | `wildhorserescue.ini`, in the data folder | After the game |
| The settings menu | F1 or Esc (controller: Back + Start); Controls, Display and Game pages; pauses the game; saves on close | The other recomps' menu (Dear ImGui), kept in step with them: see `roadmap-and-design-targets.md` |
| The Windows launchers | `Play Wild Horse Rescue.bat`, `Update Wild Horse Rescue.bat`; `Analyse game.bat` until the game runs | As `Play Ghoulies.bat` and `Update Ghoulies.bat` |
| The downloads | `BarbieWildHorseRescue--QilinRecomp-<version>-windows-x64.zip`, `-linux-x86_64.tar.gz`, `-linux-arm64.tar.gz`, `-macos.dmg` | After the project |
| The downloads' launcher | `Play Wild Horse Rescue.exe` (Windows), `Play Wild Horse Rescue` (Linux), `Wild Horse Rescue.app` (macOS) | The shared launcher's `Play <title>`; the title is the short name because the whole one has a colon, which Windows file names can't hold |
| Lifted functions | `fn_<address>` | Named by their address in the original XBE; real names come from analysis later |

The game executable is `qilin_run` while the runtime is brought up, as in the sister projects; it takes the game's name with the first player release.

New code follows the same rule: the prefix marks the toolchain, and anything a player sees is named after the game.

# Wild Horse Rescue 0.16.0 validation

This pass ports verified general runtime and launcher fixes from Ghoulies,
with Barbie's own build validation, native symbol registry and calling
conventions. Game addresses, renderer algorithms and guest timing are unchanged.
No game files were available here; no complete playthrough is claimed.

Local GCC 14 Release build, warnings as errors: all runnable CTest checks
passed (17 passed, renderer/menu preview skipped without Vulkan/display).
This includes three synthetic XBE programs lifted, compiled and run, loader/CLI
checks, x87 consistency, audio, settings and native host tests.

ASan + UBSan passed the host, audio and settings suites. LeakSanitizer was
disabled. A second supported local compiler was unavailable in this workspace;
Clang, MSVC, macOS and ARM64 are covered by CI rather than a second local build.

The new host tests create/release/reuse sound buffers of 1, 2, 3, 5, 6, 7 and
4,099 bytes through this build's native replacements, inspect physical
reservations through VirtualMemory's public interface and check every byte
before reuse. The mixer is joined before synthetic RAM is freed. Barbie's
symbol set lacks the Ghoulies Lock entry point; that API is not invented or
registered for the fixture. Existing zero-frame audio tests reproduced a real
UBSan error, now fixed without losing gain priming.

Isolated POSIX children install prior handlers, map RAM twice and verify that
SIGSEGV/SIGBUS outside guest memory reach the predecessor. Existing guest
fault tests retain their expected guest address and exit status. Windows handler
registration is compiled/tested in CI; the POSIX forwarding fixture is not a
Windows exception-handler test.

The Windows launcher regression runs real Ninja with a synthetic lifter/compiler
and no game code. It checks old-timestamp archive/header/compiler changes,
changed disc contents, unchanged reuse, forced recompilation, failed replacement
of a locked executable, subsequent recovery and literal Unicode/metacharacter
arguments to a native fixture child. This verifies launcher argument forwarding,
not all Unicode filesystem boundaries of the actual game runtime.

CI checks six compiler/platform configurations and builds the Windows kit.
Linux requires the renderer to run. Complete-game progression, real controller
and audio hardware, GPU-specific behaviour, real-disc builds and save/load
playthroughs remain outstanding. Full code coverage is not whole-game compatibility.

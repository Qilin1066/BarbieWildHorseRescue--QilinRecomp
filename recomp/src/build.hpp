// The supported build's identity and the regions of its code: the part of
// game.hpp the runtime needs too (no tool headers, no std::format).
// Empty until the first analysis (docs/xbe-analysis.md).
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "regions.hpp"

namespace qilin::game {

inline constexpr std::string_view kTitle = "Barbie Horse Adventures: Wild Horse Rescue";

// SHA-256 of the supported default.xbe (USA), lower-case hex. Empty: not
// recorded yet. `qilin info` prints a file's; once recorded, every command
// refuses another build unless given --any-build.
// (Not constexpr: code that asks whether it is recorded must not be a
// constant condition to the compiler.)
inline const std::string kSha256 = "51e6aa2a064be35414866d1f048b184f174db749a3c65156211d5307d2bda7e4";

// .text by region (game code, XAPI, the C runtime, middleware...).
// Evidence for each boundary: docs/code-analysis.md.
inline const std::vector<TextRegion> kTextRegions = {
    {"game", 0x11000, 0xE9BDC, true},          // the game: 16-byte entries, its source files' names in its assertions
    {"xapi", 0xE9BDC, 0xEFC00, false},         // XAPI: unaligned, the entry point; lifted function by function
    {"babel", 0xEFC00, 0x146506, true},        // Blitz Games' engine ("Babel"): 16-byte entries, bk/bd/ba functions, xb*.cpp
    {"libs", 0x146506, 0x160130, false},       // the C runtime and more of XAPI (saves, soundtracks): unaligned; function by function
    {"eh-funclets", 0x160130, 0x167098, true}, // the game's and engine's exception-handling funclets and handler stubs
};
}  // namespace qilin::game

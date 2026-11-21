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
inline const std::string kSha256 = "";

// .text by region (game code, XAPI, the C runtime, middleware...).
inline const std::vector<TextRegion> kTextRegions = {};
}  // namespace qilin::game

// The one build of the game this port supports, and everything the analysis
// has established about it: the single place game-specific addresses live.
//
// Every table is EMPTY until the first analysis of the owner's copy
// (docs/xbe-analysis.md, docs/roadmap-and-design-targets.md step 1). Nothing
// here is guessed and nothing is carried over from another game: with a
// table empty, the tools fall back to what the XBE itself says (the whole of
// .text is one lifted region, no function alignment is assumed) and report
// honestly, so `qilin coverage` fails until the code is accounted for.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "cfg.hpp"
#include "lifter.hpp"
#include "regions.hpp"
#include "xbe.hpp"

namespace qilin::game {

inline constexpr std::string_view kTitle = "Barbie Horse Adventures: Wild Horse Rescue";

// SHA-256 of the supported default.xbe (USA), lower-case hex. Empty: not
// recorded yet. `qilin info` prints a file's; once recorded, every command
// refuses another build unless given --any-build.
inline constexpr std::string_view kSha256 = "";

struct Range {
    uint32_t lo, hi;
};
struct Replaced {
    uint32_t addr;
    std::string_view why;
};

// .text by region (game code, XAPI, the C runtime, middleware...).
inline const std::vector<TextRegion> kTextRegions = {};
// Regions lifted whole. Empty: the whole of .text.
inline const std::vector<Range> kLiftedRegions = {};
// Regions where every function entry is aligned to kAlignment bytes (to be
// measured on the directly-called functions, never assumed).
inline const std::vector<Range> kAlignedRegions = {};
inline constexpr uint32_t kAlignment = 16;
// Library code (the C runtime, XAPI, pure-computation XDK libraries) lifted
// function by function: only what lifted code can reach.
inline const std::vector<Range> kLibraries = {};
// Single functions of natively replaced libraries that are lifted all the
// same (pure computation whose exact results matter).
inline const std::vector<uint32_t> kLiftedLibraryFunctions = {};
// Entry points the runtime calls itself.
inline const std::vector<uint32_t> kStartupRoots = {};
// Functions inside lifted regions that belong to a replaced library and that
// only it calls: not lifted, and `qilin lift` fails if lifted code can reach one.
inline const std::vector<Replaced> kReplaced = {};
// Functions the runtime replaces natively: never lifted, each needs a
// registered replacement.
inline const std::vector<Replaced> kNative = {};
// A library's own data section holding tables of its functions, and the
// range those point into ("" for none).
inline constexpr std::string_view kLibDataSection = "";
inline constexpr Range kLibDataPointers = {0, 0};

// Hand-written assembly that breaks the usual function contract (flags
// passed between routines, shared stack epilogues), reviewed one by one.
inline Conventions conventions() { return {}; }

// The control-flow pass's settings for this build of the game.
inline CfgConfig cfg_config(const Xbe& xbe) {
    CfgConfig c;
    for (const XbeSection& s : xbe.sections)
        if (s.name == c.gap_section) {
            c.dataptr_lo = s.va;
            c.dataptr_hi = s.file_end();
            if (kTextRegions.empty()) c.text_regions = {{s.name, s.va, s.file_end(), true}};
        }
    if (!kTextRegions.empty()) c.text_regions = kTextRegions;
    for (const Range& r : kAlignedRegions) c.aligned_regions.emplace_back(r.lo, r.hi);
    c.alignment = kAlignment;
    c.lib_data_section = kLibDataSection;
    c.lib_dataptr_lo = kLibDataPointers.lo;
    c.lib_dataptr_hi = kLibDataPointers.hi;
    return c;
}

}  // namespace qilin::game

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
#include <string>
#include <string_view>
#include <vector>

#include "build.hpp"
#include "cfg.hpp"
#include "lifter.hpp"
#include "regions.hpp"
#include "xbe.hpp"

namespace qilin::game {

struct Range {
    uint32_t lo, hi;
};
struct Replaced {
    uint32_t addr;
    std::string_view why;
};

// Regions lifted whole. Empty: the whole of .text.
inline const std::vector<Range> kLiftedRegions = {{0x11000, 0xE9BDC}, {0xEFC00, 0x146506}, {0x160130, 0x167098}};
// Regions where every function entry is aligned to kAlignment bytes (to be
// measured on the directly-called functions, never assumed).
inline const std::vector<Range> kAlignedRegions = {{0x11000, 0xE9BDC}, {0xEFC00, 0x146506}};
inline constexpr uint32_t kAlignment = 16;
// Library code (the C runtime, XAPI, pure-computation XDK libraries) lifted
// function by function: only what lifted code can reach.
inline const std::vector<Range> kLibraries = {
    {0xE9BDC, 0xEFC00},    // XAPI
    {0x146506, 0x160130},  // the C runtime and the rest of XAPI
    {0x17A540, 0x1A4500},  // the XGRPH section: XGraphics
    {0x1C1AE0, 0x1DAD54},  // the WMADEC section: the WMA decoder
};
// Single functions of natively replaced libraries that are lifted all the
// same (pure computation whose exact results matter).
inline const std::vector<uint32_t> kLiftedLibraryFunctions = {
    // Static initializers of the replaced libraries, which the C runtime's
    // start-up calls from its table (0x231870): each only stores into its
    // library's own data (docs/code-analysis.md).
    0x2040BD, 0x20B291, 0x20B2C1,            // XPP: its device-type tables and two static objects
    0x1A7153, 0x1A715E, 0x1AA01C, 0x1AA027,  // DirectSound: pointers to four static objects
    // Direct3D functions that are pure computation.
    0x174900,  // mov eax, 1; ret 4: the library's object creation (the engine keeps the 1 as its "Direct3D object")
};
// Entry points the runtime calls itself.
inline const std::vector<uint32_t> kStartupRoots = {};
// Functions inside lifted regions that belong to a replaced library and that
// only it calls: not lifted, and `qilin lift` fails if lifted code can reach one.
inline const std::vector<Replaced> kReplaced = {};
// Functions the runtime replaces natively: never lifted, each needs a
// registered replacement.
inline const std::vector<Replaced> kNative = {
    {0x0EBDEC, "XAPI start-up's patch of kernel code for known kernel builds (cli; mov [kernel], ...; sti)"},
    {0x0EF4E4, "XAPI swapping two descriptor-table words with interrupts off (sgdt; cli; xchg; sti): the runtime has no descriptor table"},
    {0x150018, "OutputDebugStringA: int 0x2D, the kernel debugger's print service; the runtime prints the string"},
    {0x115580, "the engine's cache flush (wbinvd; ret): nothing to flush"},
};
// A library's own data section holding tables of its functions, and the
// range those point into ("" for none).
inline constexpr std::string_view kLibDataSection = "";
inline constexpr Range kLibDataPointers = {0, 0};

// Hand-written assembly that breaks the usual function contract (flags
// passed between routines, shared stack epilogues), reviewed one by one.
// The C runtime here is the sister game's generation (LIBCMT of an XDK a few
// months older) and breaks the contract in the same places.
inline Conventions conventions() {
    constexpr uint8_t ZF = 8;
    Conventions c;
    // x87 maths: a helper loads the operand and returns ZF (it is infinite or
    // a NaN); the routine called next branches on that ZF.
    c.flags_out = {{0x149015, ZF}, {0x149058, ZF}};
    c.flags_in = {{0x1474FD, ZF}, {0x147605, ZF}};
    c.stack_glue = {
        0x146799,  // pop eax; pop ecx; xchg [esp], eax; jmp eax (calls a __finally block)
        0x146C24,  // _NLG_Notify's shared body, entered by a jump after its two pushes: pop ecx; pop ebx; ret 4
        0x146D50,  // strchr's shared exit: lea eax, [edx-1]; pop ebx; ret
        0x1478A4,  // __SEH_prolog: builds the caller's frame
        0x1478DF,  // __SEH_epilog: tears it down
        0x148CB8,  // x87 maths: pop eax (drops its return address), returns to the caller's caller
        0x14906E,  // x87 maths epilogues: pop the saved control word, return
        0x14907B,
        0x1490B9,
        0x151DD4,  // __EH_prolog
    };
    return c;
}

// The control-flow pass's settings: for the supported build, what the
// analysis established; for any other executable (--any-build, the tests'
// synthetic ones), only what its XBE says: the whole of .text one lifted
// region, nothing aligned.
inline CfgConfig cfg_config(const Xbe& xbe, bool supported) {
    CfgConfig c;
    const bool known = supported && !kTextRegions.empty();
    for (const XbeSection& s : xbe.sections)
        if (s.name == c.gap_section) {
            c.dataptr_lo = s.va;
            c.dataptr_hi = s.file_end();
            if (!known) c.text_regions = {{s.name, s.va, s.file_end(), true}};
        }
    if (!known) return c;
    c.text_regions = kTextRegions;
    for (const Range& r : kAlignedRegions) c.aligned_regions.emplace_back(r.lo, r.hi);
    c.alignment = kAlignment;
    c.lib_data_section = kLibDataSection;
    c.lib_dataptr_lo = kLibDataPointers.lo;
    c.lib_dataptr_hi = kLibDataPointers.hi;
    return c;
}

}  // namespace qilin::game

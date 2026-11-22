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
    {0x1DAD60, 0x202B3C},  // the XMV section: the movie decoder (WMV video, WMA sound), over Direct3D's surfaces and DirectSound's streams
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
    0x174910,  // D3D_GetDeviceCaps: checks its arguments, copies the capabilities table
    0x1670A0,  // D3DDevice_GetDeviceCaps: copies the capabilities table (0x24CFE0)
    // Direct3D internals the native Direct3D (runtime/host/d3d.cpp) runs as
    // the library wrote them: memory only, no GPU registers or commands.
    0x16C5D0,  // the device's default state: identity transforms, render and texture-stage state defaults
    0x16D440,  // D3D_DestroyResource
    0x16D520,  // a surface's internal reference (and its texture's)
    0x16D750,  // dropping one (freeing the resource at none)
    0x16DB50,  // a depth buffer's largest depth value, into the device (+0x944)
    0x167370,  // a depth buffer's format bits (+0x948)
    0x16DA30,  // the render target's anti-aliasing mode (+0x19FC)
    0x169BA0,  // D3D_CommonSetMultiSampleModeAndScale: the device's multisample scale and viewport
    0x16A460,  // the fixed-function vertex shader object (0x17A3B8) for an FVF
    0x16A860,  // a vertex shader declaration's constant loads (size, or written out)
    0x16A6C0,  // a vertex shader declaration into the shader object's attribute slots
    0x16A650,  // a vertex program into the shader object's push-buffer form
    0x16A5A0,  // LoadVertexShader's shadow copies
    0x16AFC0,  // the fixed-function viewport program's GPU methods (into the push-buffer sink)
    0x16D840,  // float to int, truncating (cvttss2si)
    0x16D3F0,  // a resource's private data (SetPrivateData) dropped
    // Direct3D's resource code, pure computation over XAPI's allocator
    // (XMemAlloc 0xEAF48, XMemFree 0xEAFE8): lifted, the headers, sizes,
    // pitches and swizzled layouts are exactly the original's. What it calls
    // that touches the GPU or the command buffer stays native.
    0x174940,  // D3DDevice_CreateTexture2
    0x16E0A0,  // the pixel-container header builder
    0x16D8D0,  // its helper
    0x1749F0,  // D3DTexture_GetLevelDesc (a jump to Get2DSurfaceDesc)
    0x16DD10,  // Get2DSurfaceDesc
    0x16DBC0,  // a pixel container's dimensions
    0x174A00,  // D3DTexture_GetSurfaceLevel2
    0x171A00,  // a texture's surface-level header
    0x16DDD0,  // a level's size, pitch and offset
    0x174A50,  // D3DTexture_LockRect
    0x171AB0,  // D3DSurface_LockRect
    0x16E000,  // Lock2DSurface
    0x171A90,  // D3DSurface_GetDesc
    0x16D5F0,  // D3DResource_GetType
    0x16D560,  // D3DResource_AddRef
    0x16ACD0,  // D3DDevice_SetStreamSource: the stream table (0x17A1E0), references and dirty bits; no GPU commands
    // The fixed-function material and lights: kept in the device as the
    // library keeps them, no GPU commands. (Drawing with D3DRS_LIGHTING on is
    // the native Direct3D's, which stops there until it is written.)
    0x1671E0,  // D3DDevice_SetMaterial
    0x167D80,  // D3DDevice_SetLight
    0x168040,  // D3DDevice_LightEnable
    0x16B380, 0x16B450, 0x16B4B0, 0x16B4E0,  // a light's derived values
    0x16D850, 0x16D860, 0x16D8C0,            // cosine, exponential and their like on the x87
    // DirectSound's 3D calculator, which the native DirectSound runs over the
    // library's own blocks (runtime/host/dsound.cpp, 3D): pure computation,
    // no APU registers. Its two entry points, the full-HRTF installer and the
    // ten functions it installs (0x1C1914-0x1C1938), the head-related filter
    // lookup, and what they call.
    0x1A61E9, 0x1A5224,                      // CDirectSound3DCalculator_Calculate3D, _GetVoiceData
    0x1A7981,                                // CHRTFSource_SetFullHRTF5Channel
    0x1A724D, 0x1A7296, 0x1A73F9, 0x1A748F, 0x1A74DA, 0x1A75EB, 0x1A767B, 0x1A774A, 0x1A77EC, 0x1A7865,  // its functions
    0x1AA036,                                // the head-related filter lookup
    0x1A4589, 0x1A45AE,                      // saving the x87 state inside a DPC (KeSaveFloatingPointState)
    0x1A460B, 0x1A4658, 0x1A4681, 0x1A46B8, 0x1A46D8, 0x1A470C, 0x1A472E,  // the calculator's calls through the installed functions
    0x1A7169, 0x1A7190, 0x1A71C7, 0x1A7210,  // vector maths
    0x1A98CA, 0x1A9953, 0x1AC1F3, 0x1AC1FC, 0x1AC588, 0x1AC720,  // the maths they share (logarithms, float to int)
};
// Entry points the runtime calls itself.
inline const std::vector<uint32_t> kStartupRoots = {
    // Direct3D internals the native Direct3D calls (see kLiftedLibraryFunctions).
    0x16C5D0, 0x16D440, 0x16D520, 0x16D750, 0x16DB50, 0x167370, 0x16DA30, 0x169BA0,
    0x16A460, 0x16A860, 0x16A6C0, 0x16A650, 0x16A5A0,
    0x16AFC0, 0x16D840, 0x16D3F0, 0x16D560,
    // DirectSound's 3D (see kLiftedLibraryFunctions), and XAPI's
    // XAudioGetSpeakerConfig, which DirectSoundCreate asks.
    0x1A61E9, 0x1A5224, 0x1A7981, 0x1AA036, 0x15089F,
};
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
    // x87 maths: the range check before f2xm1 jumps, out of range, to the
    // glue at 0x148CB8, which drops its return address and runs the maths'
    // shared error path in its caller's frame, returning to that caller's
    // caller (callers 0x148BD6 and exp's 0x15227E).
    c.returns_past_caller = {0x148D06};
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

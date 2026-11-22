// The game's functions the runtime replaces natively (game::kNative in
// recomp/src/game.hpp says why each can't be lifted), registered at their
// addresses in the supported build. One that isn't registered here stops
// the runtime, named, if anything calls it.
#include <cstdio>
#include <string>

#include "host.hpp"

namespace qilin::host {
namespace {

// 0x0EBDEC, XAPI start-up, no arguments: patches a word of kernel code on
// the kernel builds it knows, then (through 0x0EBD88) finds the kernel
// image's INIT section and swaps two words of the processor's descriptor
// table. There is no kernel image and no descriptor table here: nothing to do.
void xapi_patch_kernel(rt::Context& c) { ret(c, c.eax, 0); }

// 0x115580, the engine: wbinvd; ret. Nothing to flush.
void flush_caches(rt::Context& c) { ret(c, c.eax, 0); }

// 0x150018, VOID OutputDebugStringA(LPCSTR): XAPI hands the string to the
// kernel debugger (int 0x2D). The runtime prints it.
void output_debug_string_a(rt::Context& c) {
    const rt::Mem m{c.mem, c.mem_size};
    std::string text;
    for (uint32_t a = arg(c, 0); a && text.size() < 4096; ++a) {
        const char ch = char(rt::ld8(m, a, 0));
        if (!ch) break;
        text += ch;
    }
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    std::fprintf(stderr, "[guest] %s\n", text.c_str());
    ret(c, c.eax, 4);
}

}  // namespace

void register_game_natives() {
    register_hle(0x0EBDEC, "XAPI's kernel patch", xapi_patch_kernel);
    register_hle(0x115580, "the engine's cache flush", flush_caches);
    register_hle(0x150018, "OutputDebugStringA", output_debug_string_a);
    // The German loading screen (the game asks for it when its language is
    // German, which the settings can choose: game_hooks.cpp) is on European
    // discs only; the US one stands in.
    add_disc_fallback("loadingG.xbp", "Loading.xbp");
}

}  // namespace qilin::host

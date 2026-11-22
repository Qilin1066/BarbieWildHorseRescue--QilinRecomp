// Native code the lifted game runs before or after some of its own
// functions (game.hpp's conventions, before_entry and after_return): where an
// enhancement adjusts what the game's code passes or computed.
#include "host.hpp"

namespace qilin::host {
namespace {

// The game's language. This US build always sets 10, us (in its list at
// 0x24BE98: uk, f, d, e, it, nl, sw, fin, n, dk, us, jp, …), though the disc
// carries the string tables d, e, f, it, nl and uk too. 0x321C0 (from
// 0xE9BCE) sets it: the string table's package, packages\strings\stbl_<l>,
// the engine's language for its localised files (0x35AB0, 0x116590) and the
// game's object's (+0x80). The settings' language (the console's,
// XC_LANGUAGE) goes in as its argument, as a European disc's language
// selection would set it. The one file that then asks for this disc lacks,
// the German loading screen, has the US one stand in (game_native.cpp).
uint32_t string_table(uint32_t console_language) {
    switch (console_language) {
        case 3: return 2;   // German: stbl_d
        case 4: return 1;   // French: stbl_f
        case 5: return 3;   // Spanish: stbl_e
        case 6: return 4;   // Italian: stbl_it
        default: return 10;  // English: stbl_us, as the game asks
    }
}

// 0x321C0's argument, from 0xE9BCE.
void package_begins(rt::Context& c) {
    const rt::Mem m{c.mem, c.mem_size};
    if (rt::ld32(m, c.esp, 0) != 0xE9BD3u) return;
    if (rt::ld32(m, c.esp + 4, 0) != 10u) rt::trap(0x321C0u, "the string table's language isn't the 10 this build passes");
    rt::st32(m, c.esp + 4, string_table(console_language()), 0);
}

}  // namespace
}  // namespace qilin::host

namespace qilin::rt {

void after_return(Context& c, uint32_t fn) {
    switch (fn) {
        case 0xF2E60: return host::projection_built(c);
        default: trap(fn, "a function marked after_return that has no hook (host/game_hooks.cpp)");
    }
}

void before_entry(Context& c, uint32_t fn) {
    switch (fn) {
        case 0x321C0: return host::package_begins(c);
        default: break;
    }
    trap(fn, "a function marked before_entry that has no hook (host/game_hooks.cpp)");
}

}  // namespace qilin::rt

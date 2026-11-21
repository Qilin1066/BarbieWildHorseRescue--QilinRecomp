// A named part of .text: whether it is lifted, and the name reports use.
// The supported build's regions are in game.hpp.
#pragma once

#include <cstdint>
#include <string>

namespace qilin {

struct TextRegion {
    std::string name;
    uint32_t lo, hi;
    bool lifted;  // lifted as a whole region (libraries may be lifted function by function instead)
};

}  // namespace qilin

// The game as the owner has it: a disc image, an extracted disc's folder, or
// default.xbe itself. Runtime code too (no std::format).
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace qilin {

// default.xbe's bytes, or what went wrong.
std::expected<std::vector<uint8_t>, std::string> load_game_executable(const std::string& path);
// Whether `path` is a file that is not an XBE (so: to be read as a disc image).
bool is_disc_image(const std::string& path);

}  // namespace qilin

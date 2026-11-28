#include "game_file.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>

#include "path.hpp"
#include "xiso.hpp"

namespace qilin {

namespace {

std::expected<std::vector<uint8_t>, std::string> read_whole(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::unexpected("cannot open " + utf8_path(p));
    return std::vector<uint8_t>{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

bool is_disc_image(const std::string& path) {
    std::error_code ec;
    const std::filesystem::path p = native_path(path);
    if (!std::filesystem::is_regular_file(p, ec)) return false;
    std::ifstream f(p, std::ios::binary);
    char magic[4] = {};
    return f && f.read(magic, 4) && std::string_view(magic, 4) != "XBEH";
}

std::expected<std::vector<uint8_t>, std::string> load_game_executable(const std::string& path) {
    const std::filesystem::path p = native_path(path);
    std::error_code ec;
    if (std::filesystem::is_directory(p, ec)) {
        // An extracted disc: the file name's case is whatever the extractor wrote.
        for (const auto& e : std::filesystem::directory_iterator(p, ec)) {
            std::string name = utf8_path(e.path().filename());
            std::ranges::transform(name, name.begin(), [](char ch) { return ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch; });
            if (name == "default.xbe") return read_whole(e.path());
        }
        return std::unexpected(path + " holds no default.xbe");
    }
    if (!std::filesystem::exists(p, ec)) return std::unexpected("cannot open " + path);
    if (!is_disc_image(path)) return read_whole(p);
    auto iso = Xiso::open(path);
    if (!iso) return std::unexpected(path + ": " + iso.error());
    const XisoFile* f = iso->find("default.xbe");
    if (!f) return std::unexpected(path + ": the disc has no default.xbe");
    auto data = iso->read(*f);
    if (!data) return std::unexpected(path + ": " + data.error());
    return data;
}

}  // namespace qilin

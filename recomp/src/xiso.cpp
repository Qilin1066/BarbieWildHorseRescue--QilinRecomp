#include "xiso.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>

#include "path.hpp"
#include "strf.hpp"

namespace qilin {

namespace {

constexpr char kMagic[] = "MICROSOFT*XBOX*MEDIA";  // 20 bytes
constexpr size_t kMagicLen = 20;
constexpr uint32_t kVolumeSector = 32;
// Where the game partition starts: an image of the partition alone, then
// full-disc dumps of an original Xbox disc and of the later disc layouts.
constexpr uint64_t kPartitionOffsets[] = {0, 0x18300000ull, 0x0FD90000ull, 0x02080000ull};
constexpr uint8_t kAttrDirectory = 0x10;
constexpr int kMaxDepth = 64;
constexpr size_t kMaxEntries = 1u << 20;

uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }
bool same_name(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return lower(x) == lower(y); });
}

}  // namespace

std::expected<Xiso, std::string> Xiso::open(const std::string& path) {
    auto f = std::make_shared<std::ifstream>(native_path(path), std::ios::binary);
    if (!*f) return std::unexpected("cannot open " + path);
    f->seekg(0, std::ios::end);
    const std::streamoff end = f->tellg();
    if (end < 0) return std::unexpected("cannot read the size of " + path);
    return open(
        [f](uint64_t offset, std::span<uint8_t> out) {
            f->clear();
            f->seekg(std::streamoff(offset));
            f->read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
            return bool(*f) && size_t(f->gcount()) == out.size();
        },
        uint64_t(end));
}

std::expected<Xiso, std::string> Xiso::open(ReadFn read, uint64_t size) {
    Xiso x;
    x.read_ = std::move(read);
    x.size_ = size;
    uint8_t vd[kSector];
    bool found = false;
    for (uint64_t part : kPartitionOffsets) {
        const uint64_t at = part + uint64_t(kVolumeSector) * kSector;
        if (at + kSector > size) continue;
        if (!x.read_(at, vd)) return std::unexpected(strf("cannot read the volume descriptor at %#llx", static_cast<unsigned long long>(at)));
        if (std::memcmp(vd, kMagic, kMagicLen) != 0) continue;
        if (std::memcmp(vd + kSector - kMagicLen, kMagic, kMagicLen) != 0)
            return std::unexpected(strf("the volume descriptor at %#llx has no closing signature", static_cast<unsigned long long>(at)));
        x.partition_ = part;
        found = true;
        break;
    }
    if (!found) return std::unexpected(std::string("not an Xbox disc image: no volume descriptor (\"") + kMagic + "\") where one should be");
    const uint32_t root_sector = rd32(vd + 20), root_size = rd32(vd + 24);
    if (root_size != 0) {  // an empty disc has no root directory table
        if (auto r = x.walk(root_sector, root_size, "", 0); !r) return std::unexpected(r.error());
    }
    return x;
}

std::expected<void, std::string> Xiso::walk(uint32_t sector, uint32_t size, const std::string& prefix, int depth) {
    const std::string where = prefix.empty() ? std::string("the root directory") : "directory " + prefix;
    if (depth > kMaxDepth) return std::unexpected(where + " is nested too deeply (a loop?)");
    const uint64_t at = partition_ + uint64_t(sector) * kSector;
    if (at + size > size_ || size > (64u << 20)) return std::unexpected(where + strf(" (sector %u, %u bytes) is outside the image", sector, size));
    std::vector<uint8_t> dir(size);
    if (!read_(at, dir)) return std::unexpected("cannot read " + where);

    // In-order walk of the tree, without recursion; `seen` catches loops.
    struct Entry {
        std::string name;
        uint32_t sector, size;
        bool directory;
    };
    std::vector<Entry> entries;
    std::set<uint32_t> seen;
    std::vector<uint32_t> stack;  // entry offsets whose left subtree is being walked
    uint32_t cur = 0;
    bool descend = true;
    auto check = [&](uint32_t off) -> std::expected<void, std::string> {
        if (uint64_t(off) + 14 > size) return std::unexpected(where + strf(": an entry at %#x is past its end", off));
        const uint8_t len = dir[off + 13];
        if (len == 0 || uint64_t(off) + 14 + len > size) return std::unexpected(where + strf(": the entry at %#x has a bad name length", off));
        return {};
    };
    // A directory table that is empty, or whose first entry is padding, has no entries.
    if (size < 14 || rd16(dir.data()) == 0xFFFF) return {};
    while (true) {
        if (descend) {
            if (auto c = check(cur); !c) return c;
            if (!seen.insert(cur).second) return std::unexpected(where + strf(": its tree loops at %#x", cur));
            if (seen.size() > kMaxEntries) return std::unexpected(where + " has too many entries");
            const uint32_t left = rd16(&dir[cur]);
            stack.push_back(cur);
            if (left != 0 && left != 0xFFFF) {
                cur = left * 4;
                continue;
            }
        }
        if (stack.empty()) break;
        cur = stack.back();
        stack.pop_back();
        const uint8_t* e = &dir[cur];
        entries.push_back({std::string(reinterpret_cast<const char*>(e + 14), e[13]), rd32(e + 4), rd32(e + 8), (e[12] & kAttrDirectory) != 0});
        const uint32_t right = rd16(e + 2);
        if (right != 0 && right != 0xFFFF) {
            cur = right * 4;
            descend = true;
        } else {
            descend = false;
        }
    }
    for (const Entry& e : entries) {
        if (e.name.find('/') != std::string::npos || e.name.find('\\') != std::string::npos || e.name == "." || e.name == "..")
            return std::unexpected(where + ": an entry's name is not a plain file name");
        const std::string path = prefix.empty() ? e.name : prefix + "/" + e.name;
        const uint64_t off = partition_ + uint64_t(e.sector) * kSector;
        if (e.size != 0 && off + e.size > size_) return std::unexpected(path + strf(" (sector %u, %u bytes) is outside the image", e.sector, e.size));
        files_.push_back({path, off, e.size, e.directory});
        if (e.directory && e.size != 0)
            if (auto r = walk(e.sector, e.size, path, depth + 1); !r) return r;
    }
    return {};
}

const XisoFile* Xiso::find(std::string_view path) const {
    for (const XisoFile& f : files_)
        if (same_name(f.path, path)) return &f;
    return nullptr;
}

std::expected<std::vector<uint8_t>, std::string> Xiso::read(const XisoFile& f) const {
    if (f.directory) return std::unexpected(f.path + " is a directory");
    std::vector<uint8_t> out(f.size);
    if (!out.empty() && !read_(f.offset, out)) return std::unexpected("cannot read " + f.path);
    return out;
}

}  // namespace qilin

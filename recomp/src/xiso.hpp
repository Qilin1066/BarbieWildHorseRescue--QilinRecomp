// Reader for an original Xbox disc image (the Xbox DVD file system, "XISO"),
// so the tools can take the owner's dump as it is: `qilin disc`, `qilin
// extract`, and default.xbe read straight out of the image.
//
// Written from the format's public description: a volume descriptor in
// sector 32 of the game partition (20-byte magic at its start and end, the
// root directory's sector and size after the first), and directories stored
// as binary trees of entries on four-byte boundaries. The game partition
// starts at byte 0 of an image made of the game partition alone, or at a
// fixed offset in a full-disc dump; both are tried.
//
// Everything is validated: a table or file that runs outside the image, a
// tree with a loop or an entry past its directory is an error with the
// place named, never a short read.
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace qilin {

struct XisoFile {
    std::string path;      // from the root, '/' between names, as stored (no leading '/')
    uint64_t offset = 0;   // byte offset in the image
    uint32_t size = 0;
    bool directory = false;
};

class Xiso {
public:
    // Reads `out.size()` bytes at `offset`; false if they aren't all there.
    using ReadFn = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;

    static std::expected<Xiso, std::string> open(const std::string& path);
    static std::expected<Xiso, std::string> open(ReadFn read, uint64_t size);

    uint64_t partition_offset() const { return partition_; }
    // Every file and directory, directories before their contents, each
    // directory's entries in name order (the tree's own order).
    const std::vector<XisoFile>& files() const { return files_; }
    // By path, ignoring ASCII case as the Xbox does. nullptr if absent.
    const XisoFile* find(std::string_view path) const;
    std::expected<std::vector<uint8_t>, std::string> read(const XisoFile& f) const;

    static constexpr uint32_t kSector = 2048;

private:
    std::expected<void, std::string> walk(uint32_t sector, uint32_t size, const std::string& prefix, int depth);

    ReadFn read_;
    uint64_t size_ = 0, partition_ = 0;
    std::vector<XisoFile> files_;
};

}  // namespace qilin

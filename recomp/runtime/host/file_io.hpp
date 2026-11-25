#pragma once

#include <cstdint>
#include <iostream>
#include <limits>

namespace qilin::host::file_io {

inline bool flush(std::ostream& stream) {
    // A preceding read may have reached EOF; that is not a flush failure.
    if (stream.eof() && !stream.bad()) stream.clear();
    stream.flush();
    return bool(stream);
}

// NtWriteFile's all-ones offset means append. Return the resolved offset
// so the caller advances its file position from the actual end of file.
inline bool write(std::iostream& stream, uint64_t& offset, const char* bytes, uint32_t length) {
    stream.clear();
    if (offset == std::numeric_limits<uint64_t>::max()) {
        stream.seekp(0, std::ios::end);
        const std::streamoff end = stream.tellp();
        if (end < 0 || !stream) return false;
        offset = static_cast<uint64_t>(end);
    }
    if (offset > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()) - length) return false;
    stream.seekp(static_cast<std::streamoff>(offset));
    stream.write(bytes, length);
    return flush(stream);
}

}  // namespace qilin::host::file_io

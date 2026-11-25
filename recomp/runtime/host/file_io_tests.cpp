#include "file_io.hpp"

#include <cstdio>
#include <limits>
#include <sstream>
#include <streambuf>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}
struct FlushFailure : std::stringbuf {
    int sync() override { return -1; }
};
}  // namespace

int main() {
    namespace io = qilin::host::file_io;
    std::stringstream file("old", std::ios::in | std::ios::out | std::ios::binary);
    uint64_t offset = std::numeric_limits<uint64_t>::max();
    check(io::write(file, offset, "new", 3), "append succeeds");
    check(offset == 3 && file.str() == "oldnew", "append resolves the end of file");
    offset = 1;
    check(io::write(file, offset, "X", 1) && file.str() == "oXdnew", "explicit offset still works");
    offset = std::numeric_limits<uint64_t>::max() - 2;
    check(!io::write(file, offset, "bad", 3) && file.str() == "oXdnew", "invalid offset does not write");
    check(io::flush(file), "ordinary flush succeeds");
    file.seekg(0);
    char bytes[16]{};
    file.read(bytes, sizeof bytes);
    check(file.eof() && io::flush(file), "read EOF does not turn into a flush failure");
    FlushFailure buffer;
    std::iostream broken(&buffer);
    check(!io::flush(broken), "flush failure reaches the caller");
    broken.clear();
    offset = 0;
    check(!io::write(broken, offset, "save", 4), "write is unsuccessful if its flush fails");
    return failures ? 1 : 0;
}

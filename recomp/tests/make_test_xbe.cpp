// Writes the small hand-assembled Xbox programs the end-to-end tests run:
// CMake lifts each with `qilin lift`, builds it with the runtime as the game
// is built, and runs it (qilin_add_program in CMakeLists.txt).
//
//   qilin_make_test_xbe hello|missing|files <out.xbe>
//
// hello:   calls the kernel (RtlInitAnsiString) through its thunk table,
//          loops, writes its data section, and returns 42 from its entry point.
// missing: calls a kernel function the runtime doesn't provide (DbgPrint):
//          the runtime must stop and name it.
// files:   opens a file on the disc (\Device\CdRom0\HELLO.TXT) with
//          NtOpenFile, reads it with NtReadFile, closes it, and returns the
//          sum of the bytes read.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr uint32_t kBase = 0x10000, kText = 0x11000, kRdata = 0x12000, kData = 0x13000;
constexpr uint32_t kThunk = kRdata;  // the kernel thunk table opens .rdata

std::vector<uint8_t> image(0x1400, 0);
void put32(size_t o, uint32_t v) { std::memcpy(&image[o], &v, 4); }

// A tiny assembler: bytes appended to .text.
std::vector<uint8_t> code;
void b(std::initializer_list<int> bytes) {
    for (int v : bytes) code.push_back(uint8_t(v));
}
void d32(uint32_t v) {
    for (int k = 0; k < 4; ++k) code.push_back(uint8_t(v >> (8 * k)));
}
void push_imm(uint32_t v) { b({0x68}), d32(v); }
void call_import(int slot) { b({0xFF, 0x15}), d32(kThunk + 4 * uint32_t(slot)); }  // call dword [thunk]

void headers(std::initializer_list<uint32_t> ordinals) {
    std::memcpy(image.data(), "XBEH", 4);
    put32(0x104, kBase);
    put32(0x108, 0x1000);        // size of headers
    put32(0x10C, 0x4000);        // size of image
    put32(0x118, kBase + 0x400); // certificate
    put32(0x11C, 3);             // sections
    put32(0x120, kBase + 0x200); // section headers
    put32(0x128, kText ^ 0xA8FC57AB);   // entry (retail key)
    put32(0x130, 0x10000);              // stack commit
    put32(0x158, kThunk ^ 0x5B6D40B6);  // kernel thunk table (retail key)
    auto sect = [&](int i, uint32_t flags, uint32_t va, uint32_t vsize, uint32_t raw, uint32_t rsize, uint32_t name) {
        const size_t o = 0x200 + size_t(i) * 56;
        put32(o + 0, flags);
        put32(o + 4, va);
        put32(o + 8, vsize);
        put32(o + 12, raw);
        put32(o + 16, rsize);
        put32(o + 20, kBase + name);
        put32(o + 28, kBase + 0x380 + 4 * uint32_t(i));  // the head and tail shared pages' reference counts
        put32(o + 32, kBase + 0x390 + 4 * uint32_t(i));
    };
    sect(0, 2 | 4, kText, 0x100, 0x1000, 0x100, 0x300);     // preload, executable
    sect(1, 2, kRdata, 0x100, 0x1100, 0x100, 0x308);        // preload
    sect(2, 1 | 2, kData, 0x1000, 0x1200, 0x100, 0x310);    // writable, preload; the rest is BSS
    std::memcpy(&image[0x300], ".text", 6);
    std::memcpy(&image[0x308], ".rdata", 7);
    std::memcpy(&image[0x310], ".data", 6);
    put32(0x400, 0x1D0);  // the certificate's size
    put32(0x408, 0x51540001);
    const char16_t title[] = u"Qilin test";
    std::memcpy(&image[0x40C], title, sizeof title);
    size_t o = 0x1100;
    for (uint32_t ord : ordinals) put32(o, 0x80000000u | ord), o += 4;  // then a zero ends the table
}
void rdata(size_t offset, std::string_view s) { std::memcpy(&image[0x1100 + offset], s.data(), s.size()); }

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: qilin_make_test_xbe hello|missing|files <out.xbe>\n");
        return 2;
    }
    const std::string_view what = argv[1];
    if (what == "hello") {
        headers({289});  // RtlInitAnsiString
        rdata(0x40, "QILIN");
        push_imm(kRdata + 0x40);                   // the C string
        push_imm(kData + 0x20);                    // an ANSI_STRING to fill in
        call_import(0);                            // RtlInitAnsiString(&s, "QILIN")
        b({0x0F, 0xB7, 0x05}), d32(kData + 0x20);  // movzx eax, word [s.Length]      = 5
        b({0x31, 0xC9});                           // xor ecx, ecx
        const size_t loop = code.size();
        b({0x01, 0xC8});                           // add eax, ecx
        b({0x41});                                 // inc ecx
        b({0x83, 0xF9, 0x08});                     // cmp ecx, 8
        b({0x75, int(uint8_t(loop - (code.size() + 2)))});  // jne loop            eax = 5 + 0+…+7 = 33
        b({0xA3}), d32(kData + 0x800);             // mov [bss], eax
        b({0x03, 0x05}), d32(kData + 0x800);       // add eax, [bss]                  = 66
        b({0x83, 0xE8, 0x18});                     // sub eax, 24                     = 42
        b({0xC3});                                 // ret
    } else if (what == "missing") {
        headers({8});  // DbgPrint
        rdata(0x40, "hello\n");
        push_imm(kRdata + 0x40);
        call_import(0);                            // DbgPrint("hello\n"): cdecl
        b({0x83, 0xC4, 0x04});                     // add esp, 4
        b({0x31, 0xC0, 0xC3});                     // xor eax, eax; ret
    } else if (what == "files") {
        headers({202, 219, 187});  // NtOpenFile, NtReadFile, NtClose
        rdata(0x40, "\\Device\\CdRom0\\HELLO.TXT");
        // .data: an ANSI_STRING {Length, MaximumLength, Buffer} at +0x00, OBJECT_ATTRIBUTES {RootDirectory,
        // ObjectName, Attributes} at +0x10, the handle at +0x20, an IO_STATUS_BLOCK at +0x28, the buffer at +0x40.
        const uint16_t len = 24;
        std::memcpy(&image[0x1200 + 0], &len, 2);
        std::memcpy(&image[0x1200 + 2], &len, 2);
        put32(0x1200 + 4, kRdata + 0x40);
        put32(0x1200 + 0x14, kData);
        put32(0x1200 + 0x18, 0x40);  // OBJ_CASE_INSENSITIVE
        // NtOpenFile(&handle, GENERIC_READ | SYNCHRONIZE, &attributes, &iosb, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE)
        push_imm(0x60), push_imm(1), push_imm(kData + 0x28), push_imm(kData + 0x10), push_imm(0x80100000), push_imm(kData + 0x20);
        call_import(0);
        b({0x85, 0xC0});                           // test eax, eax
        b({0x75, 0x00});                           // jnz fail (patched below)
        const size_t jnz1 = code.size();
        // NtReadFile(handle, Event 0, ApcRoutine 0, ApcContext 0, &iosb, buffer, 64, ByteOffset 0)
        push_imm(0), push_imm(64), push_imm(kData + 0x40), push_imm(kData + 0x28), push_imm(0), push_imm(0), push_imm(0);
        b({0xFF, 0x35}), d32(kData + 0x20);        // push dword [handle]
        call_import(1);
        b({0x85, 0xC0});                           // test eax, eax
        b({0x75, 0x00});                           // jnz fail
        const size_t jnz2 = code.size();
        b({0xFF, 0x35}), d32(kData + 0x20);        // push dword [handle]
        call_import(2);                            // NtClose
        b({0x8B, 0x0D}), d32(kData + 0x2C);        // mov ecx, [iosb.Information]: bytes read
        b({0x31, 0xC0});                           // xor eax, eax
        b({0xBE}), d32(kData + 0x40);              // mov esi, buffer
        b({0x85, 0xC9});                           // test ecx, ecx
        b({0x74, 0x09});                           // jz done
        const size_t sum = code.size();
        b({0x0F, 0xB6, 0x16});                     // movzx edx, byte [esi]
        b({0x01, 0xD0});                           // add eax, edx
        b({0x46});                                 // inc esi
        b({0x49});                                 // dec ecx
        b({0x75, int(uint8_t(sum - (code.size() + 2)))});  // jnz sum
        b({0xC3});                                 // done: ret
        const size_t fail = code.size();
        b({0xB8}), d32(0xFFFFFFFF), b({0xC3});     // fail: mov eax, -1; ret
        code[jnz1 - 1] = uint8_t(fail - jnz1);
        code[jnz2 - 1] = uint8_t(fail - jnz2);
    } else {
        std::fprintf(stderr, "qilin_make_test_xbe: no program named %s\n", argv[1]);
        return 2;
    }
    if (code.size() > 0x100) {
        std::fprintf(stderr, "qilin_make_test_xbe: the program outgrew its section\n");
        return 1;
    }
    std::memset(&image[0x1000], 0xCC, 0x100);
    std::memcpy(&image[0x1000], code.data(), code.size());
    std::FILE* f = std::fopen(argv[2], "wb");
    if (!f || std::fwrite(image.data(), 1, image.size(), f) != image.size() || std::fclose(f) != 0) {
        std::fprintf(stderr, "qilin_make_test_xbe: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}

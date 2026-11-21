// qilin: the static recompiler for Barbie Horse Adventures: Wild Horse Rescue
// (original Xbox, USA). Work in progress.
//
// <game> is your copy: a disc image (.iso, the Xbox DVD file system), an
// extracted disc's folder, or its default.xbe.
//
//   qilin info     <game> [--json out.json]   XBE headers, sections, libraries, kernel imports, SHA-256
//   qilin imports  <game>                     the kernel functions it imports, by name
//   qilin disc     <image>                    the files on a disc image
//   qilin extract  <image> --out <dir> [--only <path>]
//                                             files from a disc image (outside the repo, or an ignored folder)
//   qilin cfg      <game> [--json out.json]   function discovery + control flow, summary
//   qilin align    <game>                     how directly-called functions are aligned, per code section
//   qilin suspects <game>                     kept functions with problems, for review
//   qilin insns    <game> [--linear] [-o out] instruction listing for the LLVM cross-check
//   qilin lift     <game> --out <dir> [--range lo-hi] [--only addr]
//                                             lift to C++ (generated code: never commit it)
//   qilin image    <game> <out.bin>           flat guest image (for the differential tests)
//   qilin coverage <game> [--list]            byte accounting: code / tables / padding / unknown
//
// Every command that reads the executable refuses a build other than the
// supported one (game.hpp) unless given --any-build; until the supported
// build's hash is recorded, any build is read and `info` says so.
// Exit status is non-zero on any error; nothing fails silently.
#include <array>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cfg.hpp"
#include "decoder.hpp"
#include "game.hpp"
#include "lifter.hpp"
#include "sha256.hpp"
#include "xbe.hpp"
#include "xiso.hpp"

#ifndef QILIN_VERSION
#define QILIN_VERSION "unknown"
#endif

using namespace qilin;
using game::Range;
using game::Replaced;

namespace {

// Everything specific to the game's build is in game.hpp.
using game::kLibraries;
using game::kNative;
using game::kReplaced;
using game::kStartupRoots;

bool is_native(uint32_t a) { return std::ranges::any_of(kNative, [&](const Replaced& r) { return r.addr == a; }); }

// Xbox kernel exports by ordinal (third_party/nxdk, CC0).
struct KernelExport {
    uint16_t ordinal;
    std::string_view name;
    uint8_t arg_bytes;
    bool data, fastcall;
};
constexpr KernelExport kKernelExports[] = {
#include "kernel_exports.inc"
};
const KernelExport* kernel_export(uint32_t ordinal) {
    for (const KernelExport& e : kKernelExports)
        if (e.ordinal == ordinal) return &e;
    return nullptr;
}

struct Failure {
    std::string message;
};

int usage() {
    std::print(stderr,
               "qilin " QILIN_VERSION "\n"
               "usage (<game>: a disc image, an extracted disc's folder, or its default.xbe):\n"
               "  qilin info     <game> [--json out.json]\n"
               "  qilin imports  <game>\n"
               "  qilin disc     <image>\n"
               "  qilin extract  <image> --out <dir> [--only <path>]\n"
               "  qilin cfg      <game> [--json out.json]\n"
               "  qilin align    <game>\n"
               "  qilin suspects <game>\n"
               "  qilin insns    <game> [--linear] [-o out.txt]\n"
               "  qilin lift     <game> --out <dir> [--range lo-hi] [--only addr]\n"
               "  qilin image    <game> <out.bin>\n"
               "  qilin coverage <game> [--list]   (exit 3 while any lifted byte is unaccounted for)\n"
               "  qilin --version\n"
               "  --any-build: read an executable that is not the supported build\n");
    return 2;
}

std::optional<std::string_view> opt(std::span<const std::string_view> a, std::string_view name) {
    for (size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i] == name) return a[i + 1];
    return std::nullopt;
}
bool flag(std::span<const std::string_view> a, std::string_view name) { return std::ranges::contains(a, name); }

std::optional<uint32_t> parse_hex(std::string_view s) {
    if (s.starts_with("0x") || s.starts_with("0X")) s.remove_prefix(2);
    uint32_t v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v, 16);
    if (ec != std::errc{} || p != s.data() + s.size()) return std::nullopt;
    return v;
}

// A file whose text is unchanged is left alone, so a build after an update
// recompiles only what the update changed.
void write_file(const std::filesystem::path& path, std::string_view s) {
    {
        std::ifstream old(path, std::ios::binary);
        if (old && std::string(std::istreambuf_iterator<char>(old), {}) == s) return;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f || !f.write(s.data(), std::streamsize(s.size()))) throw Failure{"cannot write " + path.string()};
}

using File = std::unique_ptr<FILE, int (*)(FILE*)>;
File open_out(std::optional<std::string_view> path) {
    if (!path) return File(stdout, [](FILE*) { return 0; });
    FILE* f = std::fopen(std::string(*path).c_str(), "wb");
    if (!f) throw Failure{std::format("cannot write {}", *path)};
    return File(f, &std::fclose);
}

// One line per instruction: "addr len bytes mnemonic<TAB>intel text".
void emit(FILE* out, const Decoder& dec, const Insn& i) {
    std::string hex;
    for (uint8_t b : dec.bytes(i)) std::format_to(std::back_inserter(hex), "{:02x}", b);
    std::println(out, "{:08x} {} {} {}\t{}", i.addr, i.len, hex, Decoder::mnemonic_name(i.mnem), dec.format(i));
}

// The reasons a lifted function could reach a replaced one: a direct call or
// tail call, or its address as an immediate or displacement (a function
// pointer for an indirect call). A data section holding the address could
// feed an indirect call from anywhere, so that counts too.
std::vector<std::string> check_replaced(const Xbe& xbe, Decoder& dec, const Cfg& cfg, std::span<const uint32_t> lifted) {
    std::vector<std::string> out;
    for (const Replaced& r : kReplaced) {
        if (!cfg.functions().contains(r.addr)) out.push_back(std::format("{:08X} is no longer a function", r.addr));
        for (uint32_t a : lifted) {
            const Function& fn = cfg.functions().at(a);
            if (fn.calls.contains(r.addr) || fn.tails.contains(r.addr))
                out.push_back(std::format("{:08X} calls or tail-calls {:08X}", a, r.addr));
            for (uint32_t ia : fn.insns) {
                const Insn* i = dec.decode(ia);
                for (int k = 0; i && k < i->nops; ++k) {
                    const Operand& o = i->op[k];
                    if ((o.kind == Operand::Kind::Imm && !o.imm_relative && uint32_t(o.imm) == r.addr) ||
                        (o.kind == Operand::Kind::Mem && uint32_t(o.disp) == r.addr))
                        out.push_back(std::format("{:08X} refers to {:08X} at {:08X}", a, r.addr, ia));
                }
            }
        }
        for (const XbeSection& s : xbe.sections) {
            if (s.code) continue;
            for (uint32_t va = s.va; va + 4 <= s.file_end(); ++va)
                if (xbe.u32(va) == r.addr) out.push_back(std::format("{:08X} is stored in {} at {:08X}", r.addr, s.name, va));
        }
    }
    return out;
}

// Library (CRT, XAPI) functions reachable from `from` and the start-up
// roots: direct calls and tail calls, and function addresses as immediates or
// displacements (pointers for indirect calls), followed transitively; plus
// every library function whose address is stored in a data section (the
// CRT's own pointer tables, such as the floating-point formatting hooks
// printf calls through). Natively replaced functions aren't followed.
std::vector<uint32_t> library_closure(const Xbe& xbe, Decoder& dec, const Cfg& cfg, std::span<const uint32_t> from) {
    const auto& fns = cfg.functions();
    auto in_lib = [](uint32_t a) {
        return std::ranges::any_of(kLibraries, [&](const Range& r) { return a >= r.lo && a < r.hi; }) ||
               std::ranges::contains(game::kLiftedLibraryFunctions, a);
    };
    std::set<uint32_t> seen;
    std::vector<uint32_t> work;
    auto add = [&](uint32_t t) {
        if (in_lib(t) && !is_native(t) && fns.contains(t) && seen.insert(t).second) work.push_back(t);
    };
    for (uint32_t r : kStartupRoots) add(r);
    auto refs = [&](const Function& fn) {
        for (uint32_t t : fn.calls) add(t);
        for (uint32_t t : fn.tails) add(t);
        for (uint32_t ia : fn.insns) {
            const Insn* i = dec.decode(ia);
            for (int k = 0; i && k < i->nops; ++k) {
                const Operand& o = i->op[k];
                if (o.kind == Operand::Kind::Imm && !o.imm_relative) add(uint32_t(o.imm));
                if (o.kind == Operand::Kind::Mem) add(uint32_t(o.disp));
            }
        }
    };
    for (uint32_t a : from) refs(fns.at(a));
    for (const XbeSection& s : xbe.sections) {
        if (s.code) continue;
        for (uint32_t va = s.va; va + 4 <= s.file_end(); ++va) add(xbe.u32(va));
    }
    while (!work.empty()) {
        const uint32_t a = work.back();
        work.pop_back();
        refs(fns.at(a));
    }
    return {seen.begin(), seen.end()};
}

int cmd_lift(const Xbe& xbe, Decoder& dec, std::span<const std::string_view> args) {
    auto outdir = opt(args, "--out");
    if (!outdir) return usage();
    // What is lifted whole: the regions the analysis has drawn, or until it has, all of .text.
    std::vector<Range> ranges = game::kLiftedRegions;
    if (ranges.empty())
        for (const XbeSection& sec : xbe.sections)
            if (sec.name == ".text") ranges.push_back({sec.va, sec.file_end()});
    if (auto r = opt(args, "--range")) {
        auto dash = r->find('-');
        auto lo = parse_hex(r->substr(0, dash)), hi = dash == std::string_view::npos ? std::nullopt : parse_hex(r->substr(dash + 1));
        if (!lo || !hi) return usage();
        ranges = {{*lo, *hi}};
    }
    std::optional<uint32_t> only;
    if (auto o = opt(args, "--only")) {
        only = parse_hex(*o);
        if (!only) return usage();
    }
    auto replaced = [](uint32_t a) { return std::ranges::any_of(kReplaced, [&](const Replaced& r) { return r.addr == a; }); };
    auto wanted = [&](uint32_t a) {
        if (only) return a == *only;
        return !replaced(a) && !is_native(a) && std::ranges::any_of(ranges, [&](const Range& r) { return a >= r.lo && a < r.hi; });
    };

    Cfg cfg(xbe, dec, game::cfg_config(xbe));
    cfg.run();
    const Conventions conv = game::conventions();
    Lifter lifter(dec, &conv);
    const std::filesystem::path dir(*outdir);
    std::filesystem::create_directories(dir);

    std::vector<uint32_t> done;
    std::set<uint32_t> called;  // direct call and tail-call targets of lifted code
    std::map<std::string, int> buckets;
    std::string failures, chunk;
    int nchunk = 0, in_chunk = 0, total = 0;
    auto flush = [&] {
        if (chunk.empty()) return;
        write_file(dir / std::format("lifted_{:03}.cpp", nchunk++),
                   "// Generated by qilin lift. Do not edit, do not commit.\n"
                   "#include \"guest.hpp\"\n#include \"functions.hpp\"\n\n"
                   "namespace qilin::gen {\nnamespace rt = qilin::rt;\n\n" + chunk + "}  // namespace qilin::gen\n");
        chunk.clear();
        in_chunk = 0;
    };
    int lib_failed = 0;
    auto lift_one = [&](uint32_t a, const Function& fn) {
        ++total;
        auto r = lifter.lift(fn);
        if (!r) {
            failures += r.error().message() + '\n';
            buckets[r.error().category()]++;
            return false;
        }
        chunk += *r + '\n';
        done.push_back(a);
        called.insert(fn.calls.begin(), fn.calls.end());
        called.insert(fn.tails.begin(), fn.tails.end());
        if (++in_chunk == 200) flush();
        return true;
    };
    for (auto& [a, fn] : cfg.functions())
        if (wanted(a)) lift_one(a, fn);
    // The library (CRT, XAPI) functions lifted code can reach, transitively (default run only).
    size_t lib_n = 0;
    if (!only && !opt(args, "--range")) {
        std::vector<uint32_t> lib = library_closure(xbe, dec, cfg, done);
        lib_n = lib.size();
        for (uint32_t a : lib)
            if (!lift_one(a, cfg.functions().at(a))) ++lib_failed;
        // Jumps into the middle of a lifted function (the C runtime's shared
        // exits: `call __SEH_epilog; ret` after a __finally block that the
        // CFG split off) get a function of their own from there.
        for (bool more = true; more;) {
            more = false;
            std::ranges::sort(done);
            std::vector<uint32_t> missing;
            std::ranges::set_difference(called, done, std::back_inserter(missing));
            for (uint32_t t : missing) {
                if (cfg.functions().contains(t) || is_native(t)) continue;
                auto fn = cfg.entry_inside(t);
                if (!fn) continue;
                bool inside_lifted = false;
                for (auto& [s0, f0] : cfg.functions())
                    if (t > f0.lo && t < f0.hi && std::ranges::binary_search(done, s0) && std::ranges::binary_search(f0.insns, t)) inside_lifted = true;
                if (!inside_lifted) continue;
                if (!lift_one(t, *fn)) ++lib_failed;
                more = true;
            }
        }
        std::ranges::sort(done);
    }
    flush();
    // Chunks a bigger lift wrote before (into the same folder) go, or they'd be built too.
    {
        std::error_code ec;
        std::vector<std::filesystem::path> stale;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            const std::string name = e.path().filename().string();
            int k = 0;
            if (name.size() == 14 && name.starts_with("lifted_") && name.ends_with(".cpp") &&
                std::from_chars(name.data() + 7, name.data() + 10, k).ec == std::errc{} && k >= nchunk)
                stale.push_back(e.path());
        }
        for (const auto& path : stale) std::filesystem::remove(path, ec);
    }

    // Targets without a lifted body (library code, game code that doesn't
    // lift yet) get a stub that forwards to rt::call_unlifted, so every
    // direct call links.
    std::vector<uint32_t> stubs;
    std::ranges::set_difference(called, done, std::back_inserter(stubs));
    std::string decls = "// Generated by qilin lift.\n#pragma once\n#include \"guest.hpp\"\n\nnamespace qilin::gen {\n";
    for (uint32_t a : done) decls += std::format("void {}(qilin::rt::Context&);\n", Lifter::name(a));
    decls += "// Stubs: no lifted body.\n";
    for (uint32_t a : stubs) decls += std::format("void {}(qilin::rt::Context&);\n", Lifter::name(a));
    decls += "}  // namespace qilin::gen\n";
    write_file(dir / "functions.hpp", decls);
    std::string stub_src = "// Generated by qilin lift.\n#include \"functions.hpp\"\n\nnamespace qilin::gen {\n";
    for (uint32_t a : stubs)
        stub_src += std::format("void {}(qilin::rt::Context& c) {{ qilin::rt::call_unlifted(c, 0x{:08X}u); }}\n", Lifter::name(a), a);
    stub_src += "}  // namespace qilin::gen\n";
    write_file(dir / "stubs.cpp", stub_src);

    // Dispatch table for indirect calls, sorted by address (binary-searchable).
    std::string table = "// Generated by qilin lift.\n#include \"functions.hpp\"\n\nnamespace qilin::gen {\n";
    table += "extern const Entry kFunctions[] = {\n";
    for (uint32_t a : done) table += std::format("    {{0x{:08X}u, {}}},\n", a, Lifter::name(a));
    table += std::format("    {{0u, nullptr}}}};\nextern const size_t kFunctionCount = {};\n}}  // namespace qilin::gen\n", done.size());
    write_file(dir / "table.cpp", table);
    write_file(dir / "failures.txt", failures);

    std::println("lifted {} of {} functions ({} call targets without a lifted body get stubs)", done.size(), total, stubs.size());
    if (lib_n) std::println("  {} of them are library functions that lifted code can reach", lib_n);
    if (lib_failed) {
        std::println(stderr, "qilin: error: {} reachable library functions don't lift (see failures.txt)", lib_failed);
        return 3;
    }
    if (!only) {
        size_t n = 0;
        for (const Replaced& r : kReplaced)
            n += std::ranges::any_of(ranges, [&](const Range& g) { return r.addr >= g.lo && r.addr < g.hi; });
        if (n) std::println("  {} functions in these regions belong to a replaced library and aren't lifted", n);
        if (auto bad = check_replaced(xbe, dec, cfg, done); !bad.empty()) {
            for (auto& b : bad) std::println(stderr, "qilin: error: a replaced function is reachable from lifted code: {}", b);
            return 3;
        }
    }
    std::vector<std::pair<int, std::string>> rs;
    for (auto& [w, n] : buckets) rs.emplace_back(n, w);
    std::ranges::sort(rs, std::greater{});
    for (auto& [n, w] : rs | std::views::take(25)) std::println("  {:5}  {}", n, w);
    return 0;
}

// The game as the owner has it: a disc image, an extracted disc's folder, or default.xbe.
bool is_image_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    char magic[4] = {};
    return f && f.read(magic, 4) && std::string_view(magic, 4) != "XBEH";
}

std::vector<uint8_t> read_whole(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw Failure{"cannot open " + p.string()};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> load_executable(const std::string& path) {
    const std::filesystem::path p(path);
    std::error_code ec;
    if (std::filesystem::is_directory(p, ec)) {
        // An extracted disc: the file name's case is whatever the extractor wrote.
        for (const auto& e : std::filesystem::directory_iterator(p, ec)) {
            std::string name = e.path().filename().string();
            std::ranges::transform(name, name.begin(), [](char ch) { return ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch; });
            if (name == "default.xbe") return read_whole(e.path());
        }
        throw Failure{path + " holds no default.xbe"};
    }
    if (!std::filesystem::exists(p, ec)) throw Failure{"cannot open " + path};
    if (!is_image_file(p)) return read_whole(p);
    auto iso = Xiso::open(path);
    if (!iso) throw Failure{path + ": " + iso.error()};
    const XisoFile* f = iso->find("default.xbe");
    if (!f) throw Failure{path + ": the disc has no default.xbe"};
    auto data = iso->read(*f);
    if (!data) throw Failure{path + ": " + data.error()};
    return std::move(*data);
}

int cmd_disc(const std::string& path) {
    auto iso = Xiso::open(path);
    if (!iso) throw Failure{path + ": " + iso.error()};
    uint64_t total = 0;
    size_t nfiles = 0;
    for (const XisoFile& f : iso->files()) {
        if (f.directory) {
            std::println("{:>12}  {}/", "", f.path);
        } else {
            std::println("{:>12}  {}", f.size, f.path);
            total += f.size;
            ++nfiles;
        }
    }
    std::println("{} files, {} bytes; the game partition starts at {:#x}", nfiles, total, iso->partition_offset());
    return 0;
}

int cmd_extract(const std::string& path, std::span<const std::string_view> args) {
    auto outdir = opt(args, "--out");
    if (!outdir) return usage();
    auto iso = Xiso::open(path);
    if (!iso) throw Failure{path + ": " + iso.error()};
    std::string only(opt(args, "--only").value_or(""));
    std::ranges::replace(only, '\\', '/');
    while (only.ends_with('/')) only.pop_back();
    auto lower = [](std::string t) {
        std::ranges::transform(t, t.begin(), [](char ch) { return ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch; });
        return t;
    };
    const std::string want = lower(only);
    const std::filesystem::path root(*outdir);
    size_t n = 0;
    uint64_t bytes = 0;
    for (const XisoFile& f : iso->files()) {
        const std::string lp = lower(f.path);
        if (!want.empty() && lp != want && !lp.starts_with(want + "/")) continue;
        const std::filesystem::path dest = root / std::filesystem::path(f.path);
        if (f.directory) {
            std::filesystem::create_directories(dest);
            continue;
        }
        std::filesystem::create_directories(dest.parent_path());
        auto data = iso->read(f);
        if (!data) throw Failure{path + ": " + data.error()};
        write_file(dest, std::string_view(reinterpret_cast<const char*>(data->data()), data->size()));
        ++n;
        bytes += f.size;
    }
    if (n == 0 && !want.empty()) throw Failure{std::format("{}: nothing on the disc is named {}", path, only)};
    std::println("extracted {} files, {} bytes, to {}", n, bytes, root.string());
    return 0;
}

int run(std::string_view cmd, const std::string& path, std::span<const std::string_view> args) {
    if (cmd == "disc") return cmd_disc(path);
    if (cmd == "extract") return cmd_extract(path, args);
    const std::vector<uint8_t> file = load_executable(path);
    auto loaded = Xbe::parse(file);
    if (!loaded) {
        std::println(stderr, "qilin: error: {}: {}", path, loaded.error());
        return 1;
    }
    const Xbe& xbe = *loaded;
    const std::string hash = sha256_hex(file);
    const bool recorded = !game::kSha256.empty(), supported = recorded && hash == game::kSha256;
    if (recorded && !supported && !flag(args, "--any-build")) {
        std::println(stderr,
                     "qilin: error: this is not the supported build of {} (USA).\n"
                     "  its SHA-256:  {}\n  supported:    {}\n"
                     "  (--any-build reads it all the same; nothing the project records is valid for it)",
                     game::kTitle, hash, game::kSha256);
        return 4;
    }
    if (cmd == "info") {
        const std::string js = xbe.to_json();
        if (auto p = opt(args, "--json")) write_file(std::string(*p), js);
        std::println("{}", js);
        std::println("file size: {} bytes", file.size());
        std::println("sha256: {}", hash);
        std::println("build: {}", supported ? "the supported build" : recorded ? "NOT the supported build" : "no supported build is recorded yet (game.hpp)");
        return 0;
    }
    if (cmd == "imports") {
        int unknown = 0;
        for (size_t k = 0; k < xbe.kernel_imports.size(); ++k) {
            const uint32_t ord = xbe.kernel_imports[k];
            const KernelExport* e = kernel_export(ord);
            if (!e) ++unknown;
            std::println("{:08x}  {:3}  {}{}", xbe.kthunk + 4 * uint32_t(k), ord, e ? e->name : std::string_view("UNKNOWN ORDINAL"),
                         e && e->data ? "  (data)" : "");
        }
        std::println("{} kernel imports", xbe.kernel_imports.size());
        if (unknown) {
            std::println(stderr, "qilin: error: {} imports have ordinals the kernel does not export", unknown);
            return 3;
        }
        return 0;
    }
    if (cmd == "image") {
        if (args.empty()) return usage();
        auto img = xbe.image();
        write_file(std::string(args[0]), std::string_view(reinterpret_cast<const char*>(img.data()), img.size()));
        std::println("base {:08x} size {}", xbe.base, img.size());
        return 0;
    }
    Decoder dec(xbe);
    if (cmd == "align") {
        // Where directly-called functions start, modulo 16, per code section:
        // what game.hpp's aligned regions are drawn from.
        Cfg cfg(xbe, dec, game::cfg_config(xbe));
        cfg.run();
        std::set<uint32_t> called;
        for (auto& [a, fn] : cfg.functions()) called.insert(fn.calls.begin(), fn.calls.end());
        std::map<std::string, std::array<uint32_t, 16>> hist;
        for (uint32_t t : called)
            if (const XbeSection* sec = xbe.code_section_at(t)) ++hist[sec->name][t % 16];
        std::println("{:<10} {:>7}  {:>7} {:>7} {:>7}  {}", "section", "called", "16", "8", "4", "by address modulo 16");
        for (auto& [name, h] : hist) {
            uint32_t n = 0, a8 = 0, a4 = 0;
            std::string mods;
            for (uint32_t m = 0; m < 16; ++m) {
                n += h[m];
                if (m % 8 == 0) a8 += h[m];
                if (m % 4 == 0) a4 += h[m];
                mods += std::format(" {}", h[m]);
            }
            std::println("{:<10} {:>7}  {:>6}% {:>6}% {:>6}% {}", name, n, percent(h[0], n, 1), percent(a8, n, 1), percent(a4, n, 1), mods);
        }
        return 0;
    }
    if (cmd == "cfg") {
        Cfg cfg(xbe, dec, game::cfg_config(xbe));
        cfg.run();
        std::print("{}", cfg.summary());
        if (auto p = opt(args, "--json")) write_file(std::string(*p), cfg.to_json());
        return 0;
    }
    if (cmd == "suspects") {
        Cfg cfg(xbe, dec, game::cfg_config(xbe));
        cfg.run();
        for (auto& s : cfg.suspects()) {
            const XbeSection* sec = xbe.code_section_at(s.start);
            std::println("{:08x} {:<8} {}", s.start, sec ? sec->name : "?", s.why);
        }
        for (uint32_t c : cfg.conflicts()) std::println("{:08x} call target inside another instruction", c);
        return 0;
    }
    if (cmd == "insns") {
        File out = open_out(opt(args, "-o"));
        if (flag(args, "--linear")) {
            // Linear sweep of every code section; undecodable bytes are listed as "bad"
            // with up to 15 following bytes (within the section) so a checker can confirm.
            Insn ins;
            for (auto& s : xbe.sections) {
                if (!s.code) continue;
                for (uint32_t a = s.va, end = s.file_end(); a < end;) {
                    if (dec.decode_uncached(a, ins) && ins.end() <= end) {
                        emit(out.get(), dec, ins);
                        a = ins.end();
                    } else {
                        std::string hex;
                        for (uint32_t k = 0; k < 15 && a + k < end; ++k) std::format_to(std::back_inserter(hex), "{:02x}", xbe.u8(a + k));
                        std::println(out.get(), "{:08x} 0 {} bad\t(bad)", a, hex);
                        a += 1;
                    }
                }
            }
        } else {
            Cfg cfg(xbe, dec, game::cfg_config(xbe));
            cfg.run();
            for (uint32_t a : cfg.all_instructions()) emit(out.get(), dec, *dec.decode(a));
        }
        return 0;
    }
    if (cmd == "coverage") {
        Cfg cfg(xbe, dec, game::cfg_config(xbe));
        cfg.run();
        const bool list = flag(args, "--list");
        std::println("{:<12} {:>7} {:>9} {:>8} {:>8} {:>8}  {:>8}", "region", "lifted", "code", "tables", "padding", "UNKNOWN", "accounted");
        uint64_t lifted_unknown = 0;
        for (auto& r : cfg.coverage()) {
            const uint64_t total = r.code + r.table + r.padding + r.unknown;
            std::println("{:<12} {:>7} {:>9} {:>8} {:>8} {:>8}  {:>8}%", r.name, r.lifted ? "yes" : "no", r.code, r.table,
                         r.padding, r.unknown, percent(total - r.unknown, total, 3));
            if (r.lifted) lifted_unknown += r.unknown;
            if (list && r.lifted)
                for (auto& g : r.unknown_spans) std::println("    unknown {:08x} +{}", g.addr, g.size);
        }
        std::println("unknown bytes in lifted regions: {}", lifted_unknown);
        return lifted_unknown ? 3 : 0;
    }
    if (cmd == "lift") return cmd_lift(xbe, dec, args);
    return usage();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && (std::string_view(argv[1]) == "--version" || std::string_view(argv[1]) == "version")) {
        std::println("qilin {}", QILIN_VERSION);
        return 0;
    }
    if (argc < 3) return usage();
    const std::vector<std::string_view> args(argv + 3, argv + argc);
    try {
        return run(argv[1], argv[2], args);
    } catch (const Failure& f) {
        std::println(stderr, "qilin: error: {}", f.message);
    } catch (const std::exception& e) {
        std::println(stderr, "qilin: internal error: {}", e.what());
    }
    return 1;
}

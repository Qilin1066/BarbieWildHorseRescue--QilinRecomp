// qilin_run: loads the game's executable into guest memory and runs its
// lifted entry point until it returns or reaches something the runtime
// doesn't provide yet, which it names. Built with the generated code
// (-DQILIN_GEN_DIR=..., or qilin_add_program in CMakeLists.txt).
//
// The XBE's entry point is XAPI's start-up code, lifted like the rest: it
// creates the title's main thread through the kernel, and the kernel is the
// runtime's (kernel.cpp, sched.cpp, kernel_io.cpp). What the runtime does
// itself is what the console's kernel does before the entry point runs:
// guest RAM with the image in it, the sections not preloaded held back, the
// kernel's thunk table, the first thread with its stack and TLS block.
//
// Direct3D, DirectSound and the controller library are registered here when
// they arrive (they depend on the game's XDK build: docs/roadmap...md step 3).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio.hpp"
#include "audio_out.hpp"
#include "build.hpp"
#include "game_file.hpp"
#include "host.hpp"
#include "input.hpp"
#include "sched.hpp"
#include "settings.hpp"
#include "sha256.hpp"
#include "window.hpp"
#include "xbe.hpp"

using namespace qilin;

namespace {

void push(rt::Context& c, uint32_t v) {
    c.esp -= 4;
    rt::st32(rt::Mem{c.mem, c.mem_size}, c.esp, v, 0);
}

// The XBE's TLS directory (header +0x12C): StartAddressOfRawData,
// EndAddressOfRawData, AddressOfIndex, AddressOfCallBacks, SizeOfZeroFill.
// A thread's TLS block is a pointer to its data, then the data: the template
// and the zero fill, rounded up to 16 bytes.
uint32_t tls_size(const rt::Mem& m, uint32_t dir) {
    if (!dir) return 0;
    auto ld = [&](uint32_t a) { return rt::ld32(m, a, 0); };
    return ((ld(dir + 16) - ld(dir) + ld(dir + 4) + 15) & ~15u) + 4;
}

}  // namespace

int main(int argc, char** argv) {
    const char* path = nullptr;
    const char* game_dir = nullptr;
    const char* data_dir = nullptr;
    bool any_build = false, mute = false, headless = false, fullscreen = false;
    host::D3dConfig d3d;
    std::string dump_audio;
    uint32_t audio_channels = 0;  // the device's own
    uint32_t ram_mb = host::kRetailRamMb;
    for (int k = 1; k < argc; ++k) {
        const std::string_view a = argv[k];
        if (a == "--ram-mb" && k + 1 < argc) {
            ram_mb = uint32_t(std::strtoul(argv[++k], nullptr, 10));
            if (ram_mb < host::kRetailRamMb || ram_mb > host::kMaxRamMb || ram_mb % 4) {
                std::fprintf(stderr, "qilin_run: --ram-mb takes a multiple of 4 from %u to %u\n", host::kRetailRamMb, host::kMaxRamMb);
                return 2;
            }
        } else if (a == "--game-dir" && k + 1 < argc) {
            game_dir = argv[++k];
        } else if (a == "--data-dir" && k + 1 < argc) {
            data_dir = argv[++k];
        } else if (a == "--dump-audio" && k + 1 < argc) {
            dump_audio = argv[++k];
        } else if (a == "--mute") {
            mute = true;
        } else if (a == "--audio-channels" && k + 1 < argc) {
            audio_channels = uint32_t(std::strtoul(argv[++k], nullptr, 10));
            if (audio_channels != 2 && audio_channels != 6) {
                std::fprintf(stderr, "qilin_run: --audio-channels takes 2 or 6\n");
                return 2;
            }
        } else if (a == "--press" && k + 1 < argc) {
            std::string why;
            if (!host::set_input_script(argv[++k], &why)) {
                std::fprintf(stderr, "qilin_run: --press: %s\n", why.c_str());
                return 2;
            }
        } else if (a == "--headless") {
            headless = true;
        } else if (a == "--fullscreen") {
            fullscreen = true;
        } else if (a == "--dump-frames" && k + 1 < argc) {
            d3d.dump_dir = argv[++k];
        } else if (a == "--frames" && k + 1 < argc) {
            d3d.max_frames = std::strtoull(argv[++k], nullptr, 10);
        } else if (a == "--gpu" && k + 1 < argc) {
            d3d.gpu = int(std::strtol(argv[++k], nullptr, 10));
        } else if (a == "--any-build") {
            any_build = true;
        } else if (!path) {
            path = argv[k];
        } else {
            path = nullptr;
            break;
        }
    }
    if (!path) {
        std::fprintf(stderr,
                     "usage: qilin_run [--ram-mb N] [--game-dir DIR] [--data-dir DIR] [--dump-audio FILE.wav] [--mute] [--audio-channels 2|6]\n"
                     "                 [--headless] [--fullscreen] [--dump-frames DIR] [--frames N] [--press SCRIPT] [--gpu N] [--any-build] <game>\n"
                     "  <game>           an extracted disc's folder, or its default.xbe\n"
                     "  --ram-mb N       guest RAM in MB (%u by default, as the retail Xbox)\n"
                     "  --game-dir DIR   the extracted disc (default: default.xbe's directory)\n"
                     "  --data-dir DIR   the hard disk: saves and caches (default: %s)\n"
                     "  --dump-audio FILE.wav  write everything the mixer plays there\n"
                     "  --mute           no sound output (--dump-audio still writes)\n"
                     "  --audio-channels 2 or 6 (5.1); by default 5.1 where the sound device has six or more speakers\n"
                     "  --headless       no window and no sound device (frames are still drawn, for --dump-frames)\n"
                     "  --fullscreen     start full screen\n"
                     "  --dump-frames DIR  write each frame shown there as frame_NNNNN.png\n"
                     "  --frames N       stop after N frames\n"
                     "  --press SCRIPT   controller presses on port 1 by frame: FRAME:BUTTONS[/FRAMES],... (e.g. 2100:start/8)\n"
                     "  --gpu N          the Vulkan device to draw with, by index (default: the best)\n"
                     "  --any-build      run an executable that is not the supported build\n",
                     host::kRetailRamMb, platform::default_data_folder().string().c_str());
        return 2;
    }
    // The executable, and that it is the one the lifted code was made from.
    if (is_disc_image(path)) {
        std::fprintf(stderr, "qilin_run: %s is a disc image: the runtime reads an extracted disc (qilin extract <image> --out <folder>)\n", path);
        return 2;
    }
    auto file = load_game_executable(path);
    if (!file) {
        std::fprintf(stderr, "qilin_run: %s\n", file.error().c_str());
        return 2;
    }
    const bool supported = !game::kSha256.empty() && sha256_hex(*file) == game::kSha256;
    if (!game::kSha256.empty() && !supported && !any_build) {
        std::fprintf(stderr, "qilin_run: this is not the supported build of %s (USA): its SHA-256 is %s, not %s\n",
                     std::string(game::kTitle).c_str(), sha256_hex(*file).c_str(), game::kSha256.c_str());
        return 4;
    }
    auto loaded = Xbe::parse(*file);
    if (!loaded) {
        std::fprintf(stderr, "qilin_run: %s: %s\n", path, loaded.error().c_str());
        return 2;
    }
    const Xbe& xbe = *loaded;
    host::set_memory_map(host::MemoryMap{ram_mb << 20});
    const host::MemoryMap& mm = host::memory_map();
    if (xbe.image_end() > mm.stack_bottom()) {
        std::fprintf(stderr, "qilin_run: the image ends at %08X, inside the stack\n", xbe.image_end());
        return 2;
    }

    // Guest RAM, with the image at its base address.
    // Mapped at 0 and at the kernel's two windows onto it; pages the guest
    // never touches cost nothing. The mapping lasts until the process exits.
    std::string ram_error;
    uint8_t* const ram_base = host::map_guest_ram(mm.ram_size, &ram_error);
    if (!ram_base) {
        std::fprintf(stderr, "qilin_run: can't set up %u MB of guest RAM: %s\n", ram_mb, ram_error.c_str());
        return 2;
    }
    struct {
        uint8_t* p;
        uint8_t* get() const { return p; }
    } ram{ram_base};
    std::memcpy(ram.get() + xbe.base, xbe.image().data(), xbe.image().size());
    rt::Context c{};
    c.mem = ram.get();
    c.mem_size = rt::kVirtualLimit;  // RAM, then the title's virtual memory
    const rt::Mem m{c.mem, c.mem_size};
    // Sections not marked preload stay unloaded until XeLoadSection, as the
    // kernel's loader leaves them: keep their file data and clear their memory.
    {
        std::vector<host::DemandSection> demand;
        const uint32_t headers = xbe.u32(xbe.base + 0x120);
        for (size_t i = 0; i < xbe.sections.size(); ++i) {
            const XbeSection& s = xbe.sections[i];
            host::DemandSection d{headers + 56 * uint32_t(i), s.va, s.vsize, {}};
            if (s.preload()) {
                // Loaded at boot: the kernel's loader counts it loaded once, and its first and last pages shared.
                host::preload_section(c, d.header);
            } else {
                d.bytes.assign(ram.get() + s.va, ram.get() + s.va + s.rsize);
                std::memset(ram.get() + s.va, 0, s.vsize);
            }
            demand.push_back(std::move(d));
        }
        host::set_demand_sections(std::move(demand));
    }

    // Kernel imports: a function's thunk slot keeps 0x80000000 | ordinal, which
    // rt::call_unlifted recognises; a variable's slot gets the address of a
    // zeroed 256-byte object in the kernel data area.
    uint32_t data_slot = mm.kernel_data();
    for (size_t i = 0; i < xbe.kernel_imports.size(); ++i) {
        const uint32_t ordinal = xbe.kernel_imports[i];
        const host::KernelExport* e = host::kernel_export(ordinal);
        const uint32_t slot = xbe.kthunk + 4 * uint32_t(i);
        if (e && e->data) {
            rt::st32(m, slot, data_slot, 0);
            host::init_kernel_data(c, ordinal, data_slot);
            data_slot += 256;
        } else {
            rt::st32(m, slot, host::kernel_addr(ordinal), 0);
        }
    }

    // Code regions, for naming what the game calls.
    std::vector<host::Region> regions;
    if (supported)
        for (const auto& r : game::kTextRegions) regions.push_back({r.lo, r.hi, r.name});
    for (const auto& s : xbe.sections)
        if (s.code && (s.name != ".text" || !supported || game::kTextRegions.empty())) regions.push_back({s.va, s.va + s.vsize, s.name});
    host::set_regions(std::move(regions));

    // RAM between the image and the main stack, for contiguous memory.
    host::vm().init(c.mem, (xbe.image_end() + 0xFFFFu) & ~0xFFFFu, mm.stack_bottom() & ~0xFFFFu);
    // And the title's virtual memory, above it.
    host::virtual_memory().init(c.mem, mm.ram_size, rt::kVirtualLimit, true);

    host::register_kernel();
    if (supported) {
        host::register_game_natives();
        // The settings (wildhorserescue.ini in the data folder): written with the defaults the first time.
        const std::filesystem::path data_folder = data_dir ? std::filesystem::path(data_dir) : platform::default_data_folder();
        static platform::Settings settings = platform::load_settings(data_folder);
        fullscreen = fullscreen || settings.fullscreen;
        if (!audio_channels) audio_channels = settings.channels;
        // The picture's shape: the game's own 4:3 until its cameras are understood (aspect.cpp).
        const float aspect = host::game_aspect_ratio(platform::aspect_ratio(settings, headless ? 0.0f : platform::Window::main_screen_aspect()));
        host::set_game_aspect(m, aspect, false);
        d3d.settings = &settings;
        d3d.settings_folder = data_folder.string();
        // The window: where there's no display, the game still runs and draws.
        static std::unique_ptr<platform::Window> window;
        if (!headless) {
            std::string why;
            window = platform::Window::create(std::string(game::kTitle).c_str(), uint32_t(std::lround(480.0f * aspect)), 480, fullscreen, &why);
            if (!window) std::printf("qilin_run: no window (%s); running without one\n", why.c_str());
        }
        // Controllers, read through the window's events; without a window, none.
        static std::unique_ptr<platform::Input> input;
        if (window) {
            input = std::make_unique<platform::Input>();
            input->start();
            input->set_keys(settings.keys);
            input->set_rumble(settings.rumble);
        }
        d3d.window = window.get();
        d3d.input = input.get();
        host::register_xinput(input.get());
        // Outside the partitions, so the game's disk never sees it.
        d3d.pipeline_cache = (data_folder / "vulkan_pipelines.cache").string();
        host::register_d3d(d3d);
        // Sound: the mixer's output to the default playback device, unless asked not to.
        static std::unique_ptr<platform::AudioOut> sound;
        if (!headless && !mute) {
            sound = std::make_unique<platform::AudioOut>();
            if (!sound->start(audio::kOutputRate, audio_channels)) sound.reset();
        }
        host::set_audio_surround(sound && sound->channels() == 6);
        host::register_dsound(dump_audio, sound.get());
    }
    std::filesystem::path disc = game_dir ? std::filesystem::path(game_dir)
                                 : std::filesystem::is_directory(path) ? std::filesystem::path(path)
                                                                       : std::filesystem::path(path).parent_path();
    if (disc.empty()) disc = ".";
    const std::filesystem::path data = data_dir ? std::filesystem::path(data_dir) : platform::default_data_folder();
    host::register_kernel_io({disc, data});
    std::printf("qilin_run: disc %s, hard disk %s\n", disc.string().c_str(), data.string().c_str());

    // The first thread: its information block (fs:[0] = end of the exception
    // chain) and the x87 control word the kernel starts a thread with. The
    // kernel puts the thread's TLS block at the top of its stack, below
    // StackBase, and starts the stack under it.
    const uint32_t tls = tls_size(m, xbe.tls_addr);
    host::init_main_thread(c, mm.kpcr(), mm.kthread(), mm.stack_top(), host::MemoryMap::kStackSize, tls);
    // Before init_main, which copies these registers as the starting state of
    // every thread created later and of DPCs: the kernel starts each thread
    // with this control word (all exceptions masked, 53-bit precision).
    c.fpcw = 0x027F;
    host::sched::init_main(c, mm.kthread(), host::call_guest);
    c.esp = (mm.stack_top() - tls) & ~15u;
    push(c, host::kReturnSentinel);
    host::set_current(&c);

    std::printf("qilin_run: %u MB of guest RAM, %zu replacements registered; calling the entry point at %08X\n", ram_mb, host::hle_count(),
                xbe.entry);
    std::fflush(stdout);
    rt::call(c, xbe.entry);
    if (c.ret_to != host::kReturnSentinel) rt::trap(xbe.entry, "the entry point returned somewhere else");
    std::printf("qilin_run: the entry point returned %u\n", c.eax);
    std::fflush(stdout);
    // On the console the kernel's start-up thread ends here and the threads
    // the entry point created (XAPI's: the title's main thread) carry on.
    // This thread is the host's first, which the window's events belong to
    // (macOS), so it stays: it gives the processor up for good, waking only
    // to do the host's housekeeping. QILIN_EXIT_AFTER_ENTRY=1 ends the
    // process instead (the test programs, which create no thread).
    if (const char* v = std::getenv("QILIN_EXIT_AFTER_ENTRY"); v && *v && *v != '0') std::_Exit(0);
    for (;;) {
        host::sched::sleep_until(host::sched::Clock::now() + std::chrono::milliseconds(10));
        host::idle(c);
    }
}

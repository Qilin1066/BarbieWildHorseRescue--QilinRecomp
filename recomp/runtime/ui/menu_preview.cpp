// qilin_menu_preview: the window, the renderer's present pass and the
// settings menu, with a test card where the game's picture will be. It is
// how the menu and the picture settings (scaling, smoothing, FSR, SMAA, the
// CRT look, vsync, full screen, the keys) are tried before the game runs,
// and it reads and writes the real settings file.
//
//   qilin_menu_preview [--data-dir DIR] [--frames N] [--open-menu] [--gpu N]
//
// --frames N stops after N frames (for checks without a person);
// --open-menu opens the menu at start, as F1 would.
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gfx.hpp"
#include "input.hpp"
#include "menu.hpp"
#include "settings.hpp"
#include "window.hpp"

using namespace qilin;

namespace {

constexpr uint32_t kWidth = 640, kHeight = 480;  // the console's frame

// The settings' picture, for the renderer (as the runtime applies them).
void apply(gfx::Renderer& r, platform::Input& input, const platform::Settings& s, float aspect) {
    gfx::Picture p = r.picture();
    p.scaling = gfx::Picture::Scaling(uint8_t(s.scaling));
    p.smooth = s.smooth;
    p.fsr = s.fsr;
    p.sharpness = float(s.sharpness) / 100.0f;
    p.smaa = s.smaa;
    p.look = gfx::Picture::Look(uint8_t(s.look));
    p.scanlines = float(s.scanlines) / 100.0f;
    p.mask = float(s.mask) / 100.0f;
    p.vsync = s.vsync;
    p.low_latency = s.low_latency;
    p.aspect = aspect;
    r.set_picture(p);
    r.set_anisotropy(s.anisotropy);
    input.set_keys(s.keys);
    input.set_rumble(s.rumble);
}

// A test card: colour bars, a grey ramp, a border at the frame's edge and a
// bar that moves a pixel a frame (tearing and pacing show on it).
void draw_card(gfx::Renderer& r, uint64_t frame) {
    const float black[4] = {0, 0, 0, 1};
    r.clear(nullptr, 0, gfx::kClearColor | gfx::kClearDepth | gfx::kClearStencil, black, 1.0f, 0);
    auto fill = [&](int32_t x, int32_t y, uint32_t w, uint32_t h, float red, float green, float blue) {
        const gfx::Rect rect{x, y, w, h};
        const float c[4] = {red, green, blue, 1};
        r.clear(&rect, 1, gfx::kClearColor, c, 1.0f, 0);
    };
    fill(0, 0, kWidth, kHeight, 1, 1, 1);
    fill(2, 2, kWidth - 4, kHeight - 4, 0.08f, 0.08f, 0.10f);
    static const float kBars[7][3] = {{1, 1, 1}, {1, 1, 0}, {0, 1, 1}, {0, 1, 0}, {1, 0, 1}, {1, 0, 0}, {0, 0, 1}};
    for (int k = 0; k < 7; ++k) fill(40 + 80 * k, 60, 80, 200, kBars[k][0] * 0.75f, kBars[k][1] * 0.75f, kBars[k][2] * 0.75f);
    for (int k = 0; k < 16; ++k) fill(40 + 35 * k, 280, 35, 60, float(k) / 15.0f, float(k) / 15.0f, float(k) / 15.0f);
    for (int k = 0; k < 280; k += 2) fill(40 + k, 360, 1, 40, 1, 1, 1);  // single-pixel lines: scaling and smoothing show here
    fill(int32_t(40 + frame % 560), 420, 8, 20, 1, 0.6f, 0.8f);
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path folder = platform::default_data_folder();
    long frames = -1;
    bool open_menu = false;
    int gpu = -1;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        if (a == "--data-dir" && k + 1 < argc) folder = argv[++k];
        else if (a == "--frames" && k + 1 < argc) frames = std::atol(argv[++k]);
        else if (a == "--gpu" && k + 1 < argc) gpu = std::atoi(argv[++k]);
        else if (a == "--open-menu") open_menu = true;
        else {
            std::fprintf(stderr, "usage: qilin_menu_preview [--data-dir DIR] [--frames N] [--open-menu] [--gpu N]\n");
            return 2;
        }
    }
    static platform::Settings settings = platform::load_settings(folder);
    std::printf("qilin_menu_preview: settings in %s\n", platform::settings_file(folder).string().c_str());

    const float aspect = platform::aspect_ratio(settings, platform::Window::main_screen_aspect());
    std::string why;
    std::unique_ptr<platform::Window> window = platform::Window::create(
        "Barbie Horse Adventures: Wild Horse Rescue", uint32_t(480.0f * (aspect > 0 ? aspect : 4.0f / 3.0f) + 0.5f), 480, settings.fullscreen, &why);
    if (!window) {
        std::fprintf(stderr, "qilin_menu_preview: no window: %s\n", why.c_str());
        return 77;  // nothing to show it on: skipped, not failed
    }
    platform::Input input;
    input.start();

    gfx::Config gc;
    gc.width = kWidth;
    gc.height = kHeight;
    gc.gpu = gpu;
    platform::Window* w = window.get();
    gc.surface.instance_extensions = w->vulkan_instance_extensions();
    gc.surface.loader = w->vulkan_loader();
    gc.surface.create = [w](void* instance, std::string* error) { return w->create_surface(instance, error); };
    gc.surface.size = [w](uint32_t* pw, uint32_t* ph) {
        if (w->minimized()) *pw = *ph = 0;
        else w->pixel_size(pw, ph);
    };
    std::unique_ptr<gfx::Renderer> renderer = gfx::Renderer::create(gc, &why);
    if (!renderer) {
        std::fprintf(stderr, "qilin_menu_preview: can't start the renderer: %s\n", why.c_str());
        return frames >= 0 ? 77 : 3;  // a check on a machine without Vulkan: skipped, not failed
    }
    std::printf("qilin_menu_preview: %s\n", renderer->device_name().c_str());
    auto shape = [&] {
        const float a = platform::aspect_ratio(settings, window->screen_aspect());
        return a > 0 ? a : 4.0f / 3.0f;
    };
    apply(*renderer, input, settings, shape());

    std::unique_ptr<ui::Menu> menu;
    auto last = std::chrono::steady_clock::now();
    for (uint64_t frame = 0; frames < 0 || frame < uint64_t(frames); ++frame) {
        if (menu) window->pump(&input, [&](const SDL_Event& e) { return menu->handle(e); });
        else window->pump(&input);
        if (window->quit_requested() || (menu && menu->quit_requested())) break;
        draw_card(*renderer, frame);
        if (menu) menu->tick();
        renderer->present();
        const auto now = std::chrono::steady_clock::now();
        if (menu && settings.performance_overlay)
            menu->frame_shown(std::chrono::duration<double, std::milli>(now - last).count(), {"the menu's preview: a test card, no game"});
        last = now;
        // The menu, once the window has shown a frame (it draws with the window's images).
        if (!menu) {
            auto m = std::make_unique<ui::Menu>(*window, *renderer, settings, folder,
                                                [&](const platform::Settings& s) { apply(*renderer, input, s, shape()); });
            if (!m->ok()) {
                std::fprintf(stderr, "qilin_menu_preview: the menu can't draw on this window\n");
                return 4;
            }
            menu = std::move(m);
            if (open_menu) {
                SDL_Event e;
                std::memset(&e, 0, sizeof e);
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = SDLK_F1;
                e.key.scancode = SDL_SCANCODE_F1;
                e.key.down = true;
                e.key.windowID = SDL_GetWindowID(window->sdl());
                SDL_PushEvent(&e);
            }
        }
        if (!settings.vsync) std::this_thread::sleep_for(std::chrono::milliseconds(4));  // unpaced otherwise: this is a preview
    }
    renderer->flush();
    std::printf("qilin_menu_preview: closed\n");
    return 0;
}

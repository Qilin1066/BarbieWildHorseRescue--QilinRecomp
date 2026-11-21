// The settings file: what is written reads back the same, every key is
// there, and a line that can't be read is named and left at its default.
// No window, no game files.
#include <cstdio>
#include <string>
#include <vector>

#include "settings.hpp"

using namespace qilin::platform;

namespace {

int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

bool has(const std::string& text, const char* what) { return text.find(what) != std::string::npos; }

}  // namespace

int main() {
    // The defaults: written, read back, written again: the same text, no warnings.
    {
        const Settings d = default_settings();
        const std::string text = settings_text(d);
        std::vector<std::string> warnings;
        const Settings back = parse_settings(text, warnings);
        CHECK(warnings.empty());
        CHECK(settings_text(back) == text);
        for (const char* section : {"[display]", "[controls]", "[sound]", "[console]"}) CHECK(has(text, section));
        for (const char* key : {"fullscreen =", "scaling =", "smooth =", "fsr =", "sharpness =", "antialiasing =", "performance_overlay =", "look =", "scanlines =", "mask =",
                                "vsync =", "frame_rate =", "low_latency =", "anisotropy =", "aspect =", "resolution =", "hud =", "rumble =",
                                "volume =", "channels =", "language ="})
            CHECK(has(text, key));
        // Every control of the Xbox controller has a line, and the defaults give the keyboard the face buttons.
        for (uint32_t c = 0; c < kControlCount; ++c) CHECK(has(text, control_name(Control(c))));
        CHECK(!d.keys[kCtlA].empty() && !d.keys[kCtlStart].empty());
        CHECK(has(text, "Wild Horse Rescue"));
        CHECK(settings_file("folder").filename() == "wildhorserescue.ini");
    }
    // Everything changed from its default survives the round trip.
    {
        Settings s = default_settings();
        s.fullscreen = true;
        s.scaling = Settings::Scaling::Integer;
        s.smooth = true;
        s.fsr = true;
        s.sharpness = 35;
        s.smaa = true;
        s.look = Settings::Look::Crt;
        s.scanlines = 10;
        s.mask = 90;
        s.vsync = false;
        s.frame_rate = Settings::FrameRate::Unlimited;
        s.low_latency = false;
        s.performance_overlay = true;
        s.anisotropy = 16;
        s.aspect_w = 21, s.aspect_h = 9;
        s.resolution = 3;
        s.hud = Settings::Hud::Middle;
        s.rumble = false;
        s.keys[kCtlA] = {"Z", "Return"};
        s.keys[kCtlBlack].clear();
        s.volume = 5;
        s.channels = 6;
        s.language = 4;
        std::vector<std::string> warnings;
        const Settings b = parse_settings(settings_text(s), warnings);
        CHECK(warnings.empty());
        CHECK(b.fullscreen && b.scaling == Settings::Scaling::Integer && b.smooth && b.fsr && b.sharpness == 35 && b.smaa);
        CHECK(b.look == Settings::Look::Crt && b.scanlines == 10 && b.mask == 90 && !b.vsync);
        CHECK(b.frame_rate == Settings::FrameRate::Unlimited && !b.low_latency && b.performance_overlay && b.anisotropy == 16);
        CHECK(b.aspect_w == 21 && b.aspect_h == 9 && b.resolution == 3 && b.hud == Settings::Hud::Middle);
        CHECK(!b.rumble && b.keys[kCtlA] == std::vector<std::string>({"Z", "Return"}) && b.keys[kCtlBlack].empty());
        CHECK(b.volume == 5 && b.channels == 6 && b.language == 4);
        CHECK(settings_text(b) == settings_text(s));
    }
    // Lines that can't be read: each named, each left at its default, the rest still read.
    {
        std::vector<std::string> warnings;
        const Settings d = default_settings();
        const Settings s = parse_settings("[display]\nvsync = maybe\nanisotropy = 99\nnonsense\nsharpness = 50\n"
                                          "[sound]\nvolume = 101\nchannels = 4\n[console]\nlanguage = 9\n[nowhere]\nkey = 1\n",
                                          warnings);
        CHECK(warnings.size() == 7);
        CHECK(s.vsync == d.vsync && s.anisotropy == d.anisotropy && s.volume == d.volume && s.channels == d.channels && s.language == d.language);
        CHECK(s.sharpness == 50);
        for (const std::string& w : warnings) CHECK(w.starts_with("line "));
    }
    // The picture's shape and the render resolution.
    {
        Settings s = default_settings();
        CHECK(aspect_ratio(s, 2.0f) == 2.0f);  // auto: the screen's own
        s.aspect_w = 4, s.aspect_h = 3;
        CHECK(aspect_ratio(s, 2.0f) > 1.33f && aspect_ratio(s, 2.0f) < 1.34f);
        uint32_t x = 0, y = 0;
        s.resolution = 2;
        resolution_scale(s, 4.0f / 3.0f, 1440, &x, &y);
        CHECK(x == 2 && y == 2);
        s.resolution = 0;  // auto: the nearest whole multiple of 480 lines to what is shown
        resolution_scale(s, 4.0f / 3.0f, 1440, &x, &y);
        CHECK(y == 3 && x >= 3);
    }
    // The keys the program keeps for itself can't be given to the game.
    CHECK(key_reserved("Escape") && key_reserved("F1") && key_reserved("F11") && !key_reserved("Space"));

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all settings tests passed\n");
    return 0;
}

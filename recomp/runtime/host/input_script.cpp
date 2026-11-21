// Scripted controller presses (qilin_run --press), on port 1, for runs with
// no one at the controls: headless runs and tests. See host.hpp.
#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

#include "host.hpp"
#include "input.hpp"
#include "strf.hpp"

namespace qilin::host {

namespace {

struct Press {
    uint64_t from = 0, until = 0;
    uint16_t buttons = 0;
    uint8_t analog = 0;  // bit n: analog button n fully pressed
    int16_t thumbs[4] = {};  // full deflections: left x, y, right x, y (y positive up)
};
std::vector<Press> g_script;

}  // namespace

// Port 0's state with the script's presses for the frame now shown.
platform::PadState with_script(uint32_t port, platform::PadState s) {
    if (port != 0 || g_script.empty()) return s;
    s.connected = true;
    const uint64_t now = frames_shown();
    for (const Press& p : g_script)
        if (now >= p.from && now < p.until) {
            s.buttons |= p.buttons;
            for (uint32_t i = 0; i < 8; ++i)
                if (p.analog & (1u << i)) s.analog[i] = 255;
            for (uint32_t i = 0; i < 4; ++i)
                if (p.thumbs[i]) s.thumbs[i] = p.thumbs[i];
        }
    return s;
}

uint32_t scripted_ports() { return g_script.empty() ? 0u : 1u; }

bool set_input_script(std::string_view script, std::string* why) {
    struct Name {
        std::string_view name;
        uint16_t button;
        int analog;
        int thumb = -1;     // the axis (left x, y, right x, y)
        int16_t value = 0;  // and its deflection
    };
    static constexpr Name kNames[] = {
        {"a", 0, 0}, {"b", 0, 1}, {"x", 0, 2}, {"y", 0, 3}, {"black", 0, 4}, {"white", 0, 5}, {"lt", 0, 6}, {"rt", 0, 7},
        {"up", 0x1, -1}, {"down", 0x2, -1}, {"left", 0x4, -1}, {"right", 0x8, -1}, {"start", 0x10, -1}, {"back", 0x20, -1},
        {"ls", 0x40, -1}, {"rs", 0x80, -1},
        {"ll", 0, -1, 0, -32768}, {"lr", 0, -1, 0, 32767}, {"ld", 0, -1, 1, -32768}, {"lu", 0, -1, 1, 32767},
        {"rl", 0, -1, 2, -32768}, {"rr", 0, -1, 2, 32767}, {"rd", 0, -1, 3, -32768}, {"ru", 0, -1, 3, 32767},
    };
    const auto number = [](std::string_view t, uint64_t& v) {
        const auto r = std::from_chars(t.data(), t.data() + t.size(), v);
        return r.ec == std::errc() && r.ptr == t.data() + t.size() && !t.empty();
    };
    std::vector<Press> out;
    while (!script.empty()) {
        const size_t comma = script.find(',');
        const std::string_view item = script.substr(0, comma);
        script = comma == std::string_view::npos ? std::string_view() : script.substr(comma + 1);
        const size_t colon = item.find(':'), slash = item.find('/');
        Press p;
        uint64_t hold = 6;
        if (colon == std::string_view::npos || !number(item.substr(0, colon), p.from) ||
            (slash != std::string_view::npos && (slash < colon || !number(item.substr(slash + 1), hold) || !hold))) {
            *why = strf("\"%.*s\" isn't FRAME:BUTTONS[/FRAMES]", int(item.size()), item.data());
            return false;
        }
        std::string_view names = item.substr(colon + 1, slash == std::string_view::npos ? std::string_view::npos : slash - colon - 1);
        while (!names.empty()) {
            const size_t plus = names.find('+');
            const std::string_view n = names.substr(0, plus);
            names = plus == std::string_view::npos ? std::string_view() : names.substr(plus + 1);
            const auto it = std::ranges::find(kNames, n, &Name::name);
            if (it == std::end(kNames)) {
                *why = strf("no button called \"%.*s\"", int(n.size()), n.data());
                return false;
            }
            if (it->analog >= 0) p.analog |= uint8_t(1u << it->analog);
            if (it->thumb >= 0) p.thumbs[it->thumb] = it->value;
            p.buttons |= it->button;
        }
        if (!p.buttons && !p.analog && !p.thumbs[0] && !p.thumbs[1] && !p.thumbs[2] && !p.thumbs[3]) {
            *why = strf("\"%.*s\" presses nothing", int(item.size()), item.data());
            return false;
        }
        p.until = p.from + hold;
        out.push_back(p);
    }
    g_script = std::move(out);
    return true;
}

}  // namespace qilin::host

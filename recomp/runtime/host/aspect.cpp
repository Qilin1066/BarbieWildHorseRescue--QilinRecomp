// The game's picture shape. The game draws for a 4:3 screen into its 640×480
// frame; a wider picture is the same frame shown wider, with the 3D
// camera's view widened to match (Hor+: the same vertical view, more at the
// sides).
//
// The engine keeps one camera (docs/graphics.md, "The engine's
// projection"): its fields of view at 0x28F88C (across) and 0x28F890 (up),
// from which 0xF2E60 builds the perspective matrix and multiplies it by the
// viewport's matrix (0x28F830) into 0x28F5B0. Everything 3D reads that
// matrix from there: 0xF34A0 gives it to Direct3D as the projection, the
// visibility test (0xF2FA0) and the projection of points to the screen
// (0xF30F0) read its scales. So after each build (an after-return hook,
// game.hpp's conventions) its x is squeezed by k = (4/3)/A about the
// viewport's centre: with the viewport mapping x to sx·x + tx, the x column
// becomes k·x + (1 − k)·tx·w. The vertical view is untouched, culling
// widens with it, and 4:3 leaves the matrix exactly as the game built it.
//
// The 2D projection (0x28F5F0) is left alone: the HUD and menus are laid out
// in 640×480 and placed for the wider picture by the HUD setting (d3d.cpp,
// flat draws). The console's own widescreen setting stays off: the game is
// given the 4:3 it was built for.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

#include "host.hpp"

namespace qilin::host {
namespace {

constexpr uint32_t kProjection = 0x28F5B0;  // float[4][4], row-major (row vectors: clip = v · M)
constexpr uint32_t kViewport = 0x28F830;    // the viewport's matrix, by which the perspective was multiplied
constexpr float k43 = 4.0f / 3.0f;

float g_aspect = k43;                // the shape being drawn for
bool g_have = false;                 // a projection has been built since start
std::array<uint32_t, 16> g_built{};  // the builder's own matrix, as it left it

float ldf(const rt::Mem& m, uint32_t a) { return std::bit_cast<float>(rt::ld32(m, a, 0)); }

// The builder's matrix, widened for the current shape, into guest memory.
void write_widened(const rt::Mem& m) {
    for (uint32_t i = 0; i < 16; ++i) rt::st32(m, kProjection + 4 * i, g_built[i], 0);
    if (std::fabs(g_aspect - k43) <= 1e-4f) return;
    const float k = k43 / g_aspect;
    const float tx = ldf(m, kViewport + 4 * 12);  // row 3, column 0: the viewport's x offset
    for (uint32_t r = 0; r < 4; ++r) {
        const float x = std::bit_cast<float>(g_built[4 * r]), w = std::bit_cast<float>(g_built[4 * r + 3]);
        rt::st32(m, kProjection + 16 * r, std::bit_cast<uint32_t>(k * x + (1.0f - k) * tx * w), 0);
    }
}

}  // namespace

float game_aspect_ratio(float requested) {
    if (!(requested > 0)) return 16.0f / 9.0f;
    return std::clamp(requested, k43, kMaxGameAspect);
}

void set_game_aspect(const rt::Mem& m, float aspect, bool running) {
    aspect = game_aspect_ratio(aspect);
    set_console_widescreen(false);
    if (aspect == g_aspect) return;
    g_aspect = aspect;
    // Between frames: the projection the game built last, at the new shape.
    // The engine hands it to Direct3D again with its next camera (0xF34A0).
    if (running && g_have) write_widened(m);
}

void projection_built(rt::Context& c) {
    const rt::Mem m{c.mem, c.mem_size};
    for (uint32_t i = 0; i < 16; ++i) g_built[i] = rt::ld32(m, kProjection + 4 * i, 0);
    g_have = true;
    write_widened(m);
}

}  // namespace qilin::host

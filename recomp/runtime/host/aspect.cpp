// The game's picture shape. The sister project widens its game's cameras by
// patching that game's table of film formats; where this game's engine
// keeps its projection hasn't been analysed yet (docs/roadmap-and-design-
// targets.md, step 7). Until it has, the game draws as on the console, for
// a 4:3 screen, and the picture is shown at 4:3 whatever the settings ask.
#include "host.hpp"

namespace qilin::host {

float game_aspect_ratio(float) { return 4.0f / 3.0f; }

void set_game_aspect(const rt::Mem&, float, bool) {}

}  // namespace qilin::host

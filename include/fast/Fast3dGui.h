#pragma once

#include "ship/window/gui/Gui.h"

namespace Fast {

// Before the renderer refactor, the Fast3D GUI implementation lived directly
// in Ship::Gui. Keep the newer game-side type name as an alias so current NEI
// can compile without replacing the Android-tested renderer.
using Fast3dGui = Ship::Gui;

} // namespace Fast

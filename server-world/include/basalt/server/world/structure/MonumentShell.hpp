#pragma once

#include <basalt/server/world/structure/MonumentBrush.hpp>

namespace basalt::server::world::structure {

/// parity: `MonumentBuilding.postProcess`, the monument's own body down to the pillars that carry it
/// to the sea bed; the rooms inside are separate pieces. Coordinates are the piece's own frame, 58 by
/// 23 by 58, which `StructurePieceFill` turns by the monument's orientation.
namespace monumentShell {

void place(const MonumentBrush& brush, std::int32_t seaLevel);

} // namespace monumentShell

} // namespace basalt::server::world::structure

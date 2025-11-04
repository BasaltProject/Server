#pragma once

#include <basalt/server/world/structure/StrongholdLayout.hpp>

namespace basalt::server::world::structure {

/// parity: `StrongholdPieces.Library`. Two storeys when it had room for eleven blocks, one when it
/// had to settle for six, and the tall one carries the balcony, the ladder and the second chest.
namespace strongholdLibrary {

void place(const StrongholdBrush& brush, const StrongholdPiece& piece);

} // namespace strongholdLibrary

/// parity: `StrongholdPieces.PortalRoom`. The one room a stronghold is built for, and the only one
/// whose shell is drawn without skipping air, so it always closes even inside a cave.
namespace strongholdPortalRoom {

void place(const StrongholdBrush& brush);

} // namespace strongholdPortalRoom

} // namespace basalt::server::world::structure

#pragma once

#include <basalt/server/world/structure/StrongholdLayout.hpp>

namespace basalt::server::world::structure {

/// parity: `StrongholdPieces.Library`.
namespace strongholdLibrary {

void place(const StrongholdBrush& brush, const StrongholdPiece& piece);

} // namespace strongholdLibrary

/// parity: `StrongholdPieces.PortalRoom`.
namespace strongholdPortalRoom {

void place(const StrongholdBrush& brush);

} // namespace strongholdPortalRoom

} // namespace basalt::server::world::structure

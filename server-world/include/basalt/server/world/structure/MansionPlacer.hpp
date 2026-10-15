#pragma once

#include <basalt/server/world/structure/MansionRoomCollection.hpp>

namespace basalt::server::world::structure {

/// parity: `WoodlandMansionPieces.MansionPiecePlacer.createMansion`.
namespace mansionPlacer {

void createMansion(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos origin,
                   StructureRotation rotation, const MansionGrid& mansion);

} // namespace mansionPlacer

} // namespace basalt::server::world::structure

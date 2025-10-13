#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `TerrainAdjustment`, all four of the jar's kinds.
enum class TerrainAdjustment : std::uint8_t { None, Bury, BeardThin, BeardBox, Encapsulate };

/// One placed piece the terrain has to make room for, or bury.
struct BeardifierRigid {
    structure::StructureBoundingBox box;
    std::int32_t groundLevelDelta = 0;
    TerrainAdjustment adjustment = TerrainAdjustment::BeardThin;
};

/// One jigsaw junction, which beards at four tenths of a piece's weight.
struct BeardifierJunction {
    std::int32_t x = 0;
    std::int32_t groundY = 0;
    std::int32_t z = 0;
};

/// parity: `Beardifier`'s rigid-piece and junction contributions, as a density function the router
/// adds on top of the base terrain.
[[nodiscard]] densityfunction::Function structureBeardifier(std::vector<BeardifierRigid> pieces,
                                                            std::vector<BeardifierJunction> junctions = {});

} // namespace basalt::server::world::terrain

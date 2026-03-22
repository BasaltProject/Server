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

/// The pieces and junctions one chunk's terrain has to adapt to, gathered across structures.
struct StructureBeard {
    std::vector<BeardifierRigid> pieces;
    std::vector<BeardifierJunction> junctions;

    [[nodiscard]] bool empty() const noexcept { return pieces.empty() && junctions.empty(); }
};

/// parity: `Beardifier`, the sampler bound into a chunk's context as its `beardifier` field. Null
/// when there is nothing to adapt to, which samples as zero.
[[nodiscard]] densityfunction::Sampler structureBeardifier(StructureBeard beard);

} // namespace basalt::server::world::terrain

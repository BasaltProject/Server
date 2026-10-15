#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `TerrainAdjustment`, all four of the jar's kinds.
enum class TerrainAdjustment : std::uint8_t { None, Bury, BeardThin, BeardBox, Encapsulate };

struct BeardifierRigid {
    structure::StructureBoundingBox box;
    std::int32_t groundLevelDelta = 0;
    TerrainAdjustment adjustment = TerrainAdjustment::BeardThin;
};

struct BeardifierJunction {
    std::int32_t x = 0;
    std::int32_t groundY = 0;
    std::int32_t z = 0;
};

struct StructureBeard {
    std::vector<BeardifierRigid> pieces;
    std::vector<BeardifierJunction> junctions;

    [[nodiscard]] bool empty() const noexcept { return pieces.empty() && junctions.empty(); }
};

/// parity: `Beardifier`.
[[nodiscard]] densityfunction::Sampler structureBeardifier(StructureBeard beard);

} // namespace basalt::server::world::terrain

#pragma once

#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `NoiseBasedChunkGenerator.getBaseHeight` and `getBaseColumn`: one column's own
/// `NoiseChunk`, its aquifer included, with no structure bearding the terrain.
class NoiseColumnHeights {
public:
    explicit NoiseColumnHeights(const NoiseRouter& router) : router_(&router) {}

    /// parity: `WORLD_SURFACE_WG`, one above the topmost non-air block.
    [[nodiscard]] std::int32_t surfaceHeight(std::int32_t x, std::int32_t z) const;

    /// parity: `OCEAN_FLOOR_WG`, one above the topmost block that blocks motion.
    [[nodiscard]] std::int32_t oceanFloorHeight(std::int32_t x, std::int32_t z) const;

    /// Whether the base column holds the dimension's default block here, which is what a ruined
    /// portal's corners are tested against.
    [[nodiscard]] bool isSolid(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// The base column's states from the bottom of the noise up.
    [[nodiscard]] std::vector<std::int32_t> column(std::int32_t x, std::int32_t z) const;

private:
    const NoiseRouter* router_;
};

} // namespace basalt::server::world::terrain

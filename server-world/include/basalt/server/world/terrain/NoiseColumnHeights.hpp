#pragma once

#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>

namespace basalt::server::world::terrain {

/// parity: `ChunkGenerator.getBaseHeight` over `WORLD_SURFACE_WG`, at any column. A sampler per
/// query and deliberately no cache: `DensityChunkSampler` reuses one context, so sharing one across
/// concurrently generating chunks hands back wrong heights rather than crashing.
class NoiseColumnHeights {
public:
    explicit NoiseColumnHeights(const OverworldNoiseRouter& router) : router_(&router) {}

    /// The first free block above the ground at this column.
    [[nodiscard]] std::int32_t surfaceHeight(std::int32_t x, std::int32_t z) const;

    /// parity: `OCEAN_FLOOR_WG`, the solid floor with no sea-level floor under it. `surfaceHeight`
    /// lifts an open column to sea level because the aquifer fills it; this one must not, or a
    /// shipwreck meant for the sea bed would settle on the surface of the water.
    [[nodiscard]] std::int32_t oceanFloorHeight(std::int32_t x, std::int32_t z) const;

    /// Whether the density says this block is solid, which is what a portal's corners are tested
    /// against.
    [[nodiscard]] bool isSolid(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    const OverworldNoiseRouter* router_;
};

} // namespace basalt::server::world::terrain

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/NoiseChunk.hpp>
#include <basalt/server/world/terrain/NoiseColumnHeights.hpp>

namespace basalt::server::world::terrain {

std::vector<std::int32_t> NoiseColumnHeights::column(std::int32_t x, std::int32_t z) const {
    NoiseChunk noiseChunk{*router_, nullptr,
                          densityfunction::DensityVolume{1, router_->height, 1, x, router_->minY, z}};
    const std::vector<float> density = noiseChunk.finalDensity();
    std::vector<std::int32_t> states(density.size());
    // parity: top down, the order the aquifer's own caches fill in.
    for (std::int32_t y = router_->height - 1; y >= 0; --y) {
        const std::optional<std::int32_t> substance = noiseChunk.aquifer().computeSubstance(
            x, router_->minY + y, z, static_cast<double>(density[static_cast<std::size_t>(y)]));
        states[static_cast<std::size_t>(y)] = substance.value_or(router_->defaultBlock);
    }
    return states;
}

std::int32_t NoiseColumnHeights::surfaceHeight(std::int32_t x, std::int32_t z) const {
    const std::vector<std::int32_t> states = column(x, z);
    for (std::int32_t y = router_->height - 1; y >= 0; --y) {
        if (!registry::blocks::isAir(states[static_cast<std::size_t>(y)])) {
            return router_->minY + y + 1;
        }
    }
    return router_->minY;
}

std::int32_t NoiseColumnHeights::oceanFloorHeight(std::int32_t x, std::int32_t z) const {
    const std::vector<std::int32_t> states = column(x, z);
    for (std::int32_t y = router_->height - 1; y >= 0; --y) {
        const std::int32_t state = states[static_cast<std::size_t>(y)];
        if (!registry::blocks::isAir(state) && !registry::blockFluids::hasFluid(state)) {
            return router_->minY + y + 1;
        }
    }
    return router_->minY;
}

bool NoiseColumnHeights::isSolid(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (y < router_->minY || y >= router_->minY + router_->height) {
        return false;
    }
    return column(x, z)[static_cast<std::size_t>(y - router_->minY)] == router_->defaultBlock;
}

} // namespace basalt::server::world::terrain

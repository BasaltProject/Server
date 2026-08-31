#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::world::terrain {

/// parity: `OreVeinifier`, applied after the aquifer material rule and before default stone.
class OverworldOreVeinifier {
public:
    OverworldOreVeinifier(const OverworldNoiseRouter& router, std::int64_t seed,
                          densityfunction::DensityCache* densityCache = nullptr);

    /// Empty for default stone, or the exact ore, raw block or filler replacement.
    [[nodiscard]] std::optional<std::int32_t> compute(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    const OverworldNoiseRouter* router_;
    densityfunction::DensityCache* densityCache_;
    random::PositionalRandomFactory positionalRandomFactory_;
};

} // namespace basalt::server::world::terrain

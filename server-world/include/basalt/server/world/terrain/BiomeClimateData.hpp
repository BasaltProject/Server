#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain {

/// parity: the temperature and temperature-modifier calls in `OverworldBiomes`/`BiomeData.bootstrap`.
///
/// Downfall stays omitted: neither temperature nor the top-layer freezing feature reads it.
struct BiomeClimate {
    float temperature = 0.5F;
    bool frozen = false;
    bool hasPrecipitation = true;
};

/// `id` is a full `namespace:path` identifier; an unknown biome falls back to 0.5, vanilla's own
/// `Biome` default.
[[nodiscard]] BiomeClimate biomeClimateFor(std::string_view id);

} // namespace basalt::server::world::terrain

#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain {

/// parity: the temperature and temperature-modifier calls in `OverworldBiomes`/`BiomeData.bootstrap`.
struct BiomeClimate {
    float temperature = 0.5F;
    bool frozen = false;
    bool hasPrecipitation = true;
};

[[nodiscard]] BiomeClimate biomeClimateFor(std::string_view id);

} // namespace basalt::server::world::terrain

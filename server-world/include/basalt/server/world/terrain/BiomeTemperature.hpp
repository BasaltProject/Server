#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::biomeTemperature {

/// parity: `Biome.getTemperature(BlockPos, int)`.
[[nodiscard]] float temperatureOf(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Biome.coldEnoughToSnow(BlockPos, int)`.
[[nodiscard]] bool coldEnoughToSnow(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Biome.shouldMeltFrozenOceanIcebergSlightly`, a temperature above 0.1.
[[nodiscard]] bool shouldMeltFrozenOceanIcebergSlightly(std::string_view biomeId, std::int32_t x, std::int32_t y,
                                                        std::int32_t z);

} // namespace basalt::server::world::terrain::biomeTemperature

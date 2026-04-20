#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/RandomState.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain {

/// parity: `TheEndBiomeSource`.
class TheEndBiomeSource {
public:
    explicit TheEndBiomeSource(const NoiseRouter& router, std::optional<std::int64_t> biomeZoomSeed = std::nullopt)
        : erosion_(router.state->sampler(router.erosion)),
          zoomer_(biomeZoomSeed.has_value() ? std::optional<BiomeZoomer>{BiomeZoomer{*biomeZoomSeed}} : std::nullopt) {}

    [[nodiscard]] std::string_view biomeAtQuart(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const;

    /// parity: `BiomeManager.getBiome`, which the placed-feature biome filters go through.
    [[nodiscard]] std::string_view blockBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    densityfunction::Sampler erosion_;
    std::optional<BiomeZoomer> zoomer_;
};

} // namespace basalt::server::world::terrain

#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain {

using NetherBiomeLookup =
    std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t, densityfunction::SamplerContext*)>;

/// parity: `MultiNoiseBiomeSourceParameterList.Preset.NETHER`.
class NetherBiomeSource {
public:
    explicit NetherBiomeSource(const NoiseRouter& router, std::optional<std::int64_t> biomeZoomSeed = std::nullopt);

    [[nodiscard]] std::string_view biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                           densityfunction::SamplerContext* cache = nullptr) const;

    [[nodiscard]] std::string_view biomeAtQuart(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                densityfunction::SamplerContext* cache = nullptr) const;

    /// parity: `BiomeManager.getBiome`, which the placed-feature biome filters go through.
    [[nodiscard]] std::string_view blockBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                densityfunction::SamplerContext* cache = nullptr) const;

    [[nodiscard]] NetherBiomeLookup cached(densityfunction::SamplerContext& context) const;

    [[nodiscard]] NetherBiomeLookup lookup() const;

private:
    struct State;

    std::shared_ptr<const State> state_;
};

} // namespace basalt::server::world::terrain

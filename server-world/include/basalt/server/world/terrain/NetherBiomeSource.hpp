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

/// The block-position biome lookup the Nether placed-feature filters read.
using NetherBiomeLookup =
    std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t, densityfunction::SamplerContext*)>;

/// parity: `MultiNoiseBiomeSourceParameterList.Preset.NETHER`. Resolves the five Nether biomes from
/// the router's temperature and vegetation noises using vanilla's own five parameter points,
/// including the non-zero offsets that bias warped forest and basalt deltas.
class NetherBiomeSource {
public:
    explicit NetherBiomeSource(const NoiseRouter& router, std::optional<std::int64_t> biomeZoomSeed = std::nullopt);

    /// The direct noise-biome lookup from block coordinates, without the biome manager's zoom.
    [[nodiscard]] std::string_view biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                           densityfunction::SamplerContext* cache = nullptr) const;

    [[nodiscard]] std::string_view biomeAtQuart(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                densityfunction::SamplerContext* cache = nullptr) const;

    /// parity: `BiomeManager.getBiome`, which the placed-feature biome filters go through.
    [[nodiscard]] std::string_view blockBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                densityfunction::SamplerContext* cache = nullptr) const;

    /// A lookup that carries one generation worker's raw-quart cache, shared by every Nether
    /// decoration in its chunk.
    [[nodiscard]] NetherBiomeLookup cached(densityfunction::SamplerContext& context) const;

    /// The plain lookup, for callers with no cache of their own.
    [[nodiscard]] NetherBiomeLookup lookup() const;

private:
    struct State;

    std::shared_ptr<const State> state_;
};

} // namespace basalt::server::world::terrain

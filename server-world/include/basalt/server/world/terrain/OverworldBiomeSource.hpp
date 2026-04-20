#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/OverworldBiomes.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace basalt::server::world::terrain {

/// parity: `MultiNoiseBiomeSource`.
class OverworldBiomeSource {
public:
    explicit OverworldBiomeSource(const NoiseRouter& router, std::optional<std::int64_t> biomeZoomSeed = std::nullopt);

    [[nodiscard]] climate::BiomeName biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             densityfunction::SamplerContext* cache = nullptr) const;

    [[nodiscard]] climate::BiomeName biomeAtQuart(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::SamplerContext* cache = nullptr) const;

    /// parity: `BiomeManager.getBiome`, used by block-position gameplay and worldgen filters.
    [[nodiscard]] climate::BiomeName blockBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                  densityfunction::SamplerContext* cache = nullptr) const;

private:
    std::optional<BiomeZoomer> zoomer_;
    std::shared_ptr<const climate::RTree<climate::BiomeName>> parameters_;
    climate::Sampler sampler_;
};

} // namespace basalt::server::world::terrain

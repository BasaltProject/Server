#pragma once

#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace basalt::server::world::terrain::surface {

/// Caches raw quart biomes while exposing BiomeManager's zoomed block-position lookup.
class ChunkBiomeEnvironment final : public SurfaceRuleEnvironment {
public:
    ChunkBiomeEnvironment(const SurfaceRuleEnvironment& delegate, densityfunction::DensityCache* densityCache);

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return delegate_->biomeZoomSeed(); }

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override {
        return delegate_->surfaceDepthNoise(x, z);
    }

    [[nodiscard]] std::int32_t preliminarySurfaceLevel(std::int32_t x, std::int32_t z) const override {
        return delegate_->preliminarySurfaceLevel(x, z);
    }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override {
        return delegate_->surfaceSecondaryNoise(x, z);
    }

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override {
        return delegate_->sample2d(noiseId, x, z);
    }

    [[nodiscard]] double sample2dScaled(std::string_view noiseId, double x, double z) const override {
        return delegate_->sample2dScaled(noiseId, x, z);
    }

    [[nodiscard]] double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                  std::int32_t z) const override {
        return delegate_->sample3d(noiseId, x, y, z);
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                               std::int32_t z) const override {
        return delegate_->verticalGradientRandom(name, x, y, z);
    }

    [[nodiscard]] random::RandomSource positionalRandom(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return delegate_->positionalRandom(x, y, z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const override {
        return delegate_->estimatedSurfaceHeight(x, z);
    }

    [[nodiscard]] bool isSteep(std::int32_t x, std::int32_t z) const override { return delegate_->isSteep(x, z); }

    [[nodiscard]] std::int32_t clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return delegate_->clayBand(x, y, z);
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override;

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             densityfunction::DensityCache* cache) const override {
        (void)cache;
        return biomeAt(x, y, z);
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::DensityCache* cache = nullptr) const override;

private:
    const SurfaceRuleEnvironment* delegate_;
    densityfunction::DensityCache* densityCache_;
    std::optional<BiomeZoomer> zoomer_;
    // Mutable because the lookups are logically const: the cache is memoization, not state.
    mutable std::unordered_map<std::int64_t, std::string> noiseBiomes_;
};

} // namespace basalt::server::world::terrain::surface

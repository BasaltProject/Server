#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::world::densityfunction {
class SamplerContext;
}

namespace basalt::server::world::terrain::surface {

/// A dimension's biome lookup as the generator's later steps see it: the noise biome at a quart and
/// the zoomed biome at a block, both through the climate sampler. Named for the surface rules it
/// first served; decoration reads it too.
class SurfaceRuleEnvironment {
public:
    SurfaceRuleEnvironment() = default;
    SurfaceRuleEnvironment(const SurfaceRuleEnvironment&) = default;
    SurfaceRuleEnvironment(SurfaceRuleEnvironment&&) = default;
    SurfaceRuleEnvironment& operator=(const SurfaceRuleEnvironment&) = default;
    SurfaceRuleEnvironment& operator=(SurfaceRuleEnvironment&&) = default;
    virtual ~SurfaceRuleEnvironment() = default;

    /// The already SHA-256-obfuscated world seed BiomeManager's Voronoi zoom uses.
    [[nodiscard]] virtual std::optional<std::int64_t> biomeZoomSeed() const { return std::nullopt; }

    /// Seed-zoomed block-position biome, matching `BiomeManager.getBiome`.
    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    /// `biomeAt` sampling the climate through one chunk's own context.
    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                     densityfunction::SamplerContext* context) const {
        (void)context;
        return biomeAt(x, y, z);
    }

    /// Direct noise-biome lookup in quart coordinates, for chunk biome palettes.
    [[nodiscard]] virtual const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                          densityfunction::SamplerContext* context = nullptr) const = 0;
};

} // namespace basalt::server::world::terrain::surface

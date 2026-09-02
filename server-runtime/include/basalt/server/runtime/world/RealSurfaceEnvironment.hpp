#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/runtime/world/RealNoiseParameters.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>
#include <basalt/server/world/terrain/surface/BadlandsClayBands.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// The real `SurfaceRuleEnvironment`: noise parameters out of the operator's own jar, the
/// per-world random, the router's own `finalDensity` for the surface height, and biomes through
/// `OverworldBiomeSource` rather than one fixed name.
///
/// `isSteep` always answers false here: vanilla clamps its neighbour lookup to the chunk under
/// construction, and this object spans every chunk. `RealTerrainGenerator` wraps it in a
/// chunk-scoped environment that implements it.
class RealSurfaceEnvironment final : public server::world::terrain::surface::SurfaceRuleEnvironment {
public:
    /// The router is borrowed, not copied: it is one immutable tree shared by every generation
    /// worker, and it has to outlive this.
    RealSurfaceEnvironment(RealNoiseParameters::ReadText readText, RealNoiseParameters::ParseParameters parseParameters,
                           server::random::PositionalRandomFactory random,
                           const server::world::terrain::OverworldNoiseRouter& router, std::int64_t biomeZoomSeed);

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return biomeZoomSeed_; }

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] std::int32_t preliminarySurfaceLevel(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] double sample2dScaled(std::string_view noiseId, double x, double z) const override;

    [[nodiscard]] double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                  std::int32_t z) const override;

    [[nodiscard]] float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                               std::int32_t z) const override;

    [[nodiscard]] server::random::RandomSource positionalRandom(std::int32_t x, std::int32_t y,
                                                                std::int32_t z) const override;

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override;

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             server::world::densityfunction::DensityCache* cache) const override;

    [[nodiscard]] const std::string&
    noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                 server::world::densityfunction::DensityCache* cache = nullptr) const override;

    /// parity: vanilla clamps its neighbour lookup to the chunk under construction, and this object
    /// spans every chunk, so it never has the neighbour to compare against.
    [[nodiscard]] bool isSteep(std::int32_t, std::int32_t) const override { return false; }

    [[nodiscard]] std::int32_t clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const override;

private:
    [[nodiscard]] double noiseAt(std::string_view noiseId, double x, double y, double z) const;

    /// The interface hands biome names out by reference, so each one is kept once and returned.
    [[nodiscard]] const std::string& nameOf(std::string_view biome) const;

    std::int64_t biomeZoomSeed_;
    server::random::PositionalRandomFactory random_;
    const server::world::terrain::OverworldNoiseRouter* router_;
    server::world::terrain::OverworldBiomeSource biomeSource_;
    // Mutable because every lookup here is memoization rather than state: the noise cache has its
    // own lock, and the two below share this one.
    mutable RealNoiseParameters noises_;
    mutable std::mutex mutex_;
    /// One factory per name, not per block: surface rules ask for nearly every block near the
    /// surface, and each call otherwise hashed a fresh string and built two generators for something
    /// that depends only on the name.
    mutable std::map<std::string, server::random::PositionalRandomFactory, std::less<>> gradientRandoms_;
    mutable std::map<std::string, std::string, std::less<>> biomeNames_;
    server::world::terrain::surface::BadlandsClayBands clayBands_;
};

} // namespace basalt::server::runtime::world

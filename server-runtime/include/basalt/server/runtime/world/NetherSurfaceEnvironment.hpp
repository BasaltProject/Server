#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/runtime/world/RealNoiseParameters.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {

/// The named noises, random streams and biomes the Nether's surface rules read.
///
/// The same shape as `RealSurfaceEnvironment`, against the Nether's own router and biome source, and
/// with a surface scan bounded by the Nether's own roof rather than the Overworld's.
class NetherSurfaceEnvironment final : public server::world::terrain::surface::SurfaceRuleEnvironment {
public:
    /// The router and the biome source are borrowed, not copied: both are immutable trees shared by
    /// every generation worker, and both have to outlive this.
    NetherSurfaceEnvironment(RealNoiseParameters::ReadText readText,
                             RealNoiseParameters::ParseParameters parseParameters,
                             server::random::PositionalRandomFactory random,
                             const server::world::terrain::NetherNoiseRouter& router,
                             const server::world::terrain::NetherBiomeSource& biomeSource, std::int64_t biomeZoomSeed);

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return biomeZoomSeed_; }

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override;

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override;

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

private:
    [[nodiscard]] double noiseAt(std::string_view noiseId, double x, double y, double z) const;

    /// The interface hands biome names out by reference, so each one is kept once and returned.
    [[nodiscard]] const std::string& nameOf(std::string_view biome) const;

    std::int64_t biomeZoomSeed_;
    server::random::PositionalRandomFactory random_;
    const server::world::terrain::NetherNoiseRouter* router_;
    const server::world::terrain::NetherBiomeSource* biomeSource_;
    // Mutable because every lookup here is memoization rather than state.
    mutable RealNoiseParameters noises_;
    mutable std::mutex mutex_;
    mutable std::map<std::string, server::random::PositionalRandomFactory, std::less<>> gradientRandoms_;
    mutable std::map<std::string, std::string, std::less<>> biomeNames_;
};

} // namespace basalt::server::runtime::world

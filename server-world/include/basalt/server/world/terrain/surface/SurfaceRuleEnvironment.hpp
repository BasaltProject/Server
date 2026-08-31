#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/densityfunction/DensityCache.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::surface {

/// parity: `net.minecraft.world.level.levelgen.WorldGenerationContext`, just the pair
/// `VerticalAnchor::resolveY` needs.
struct WorldGenerationContext {
    std::int32_t minGenY = 0;
    std::int32_t genDepth = 0;
};

/// parity: `net.minecraft.world.level.levelgen.VerticalAnchor`, a Y expressed relative to the
/// world's own bounds.
class VerticalAnchor {
public:
    [[nodiscard]] static constexpr VerticalAnchor absolute(std::int32_t y) noexcept { return {Kind::Absolute, y}; }

    [[nodiscard]] static constexpr VerticalAnchor aboveBottom(std::int32_t offset) noexcept {
        return {Kind::AboveBottom, offset};
    }

    [[nodiscard]] static constexpr VerticalAnchor belowTop(std::int32_t offset) noexcept {
        return {Kind::BelowTop, offset};
    }

    [[nodiscard]] static constexpr VerticalAnchor bottom() noexcept { return aboveBottom(0); }

    [[nodiscard]] static constexpr VerticalAnchor top() noexcept { return belowTop(0); }

    [[nodiscard]] constexpr std::int32_t resolveY(const WorldGenerationContext& context) const noexcept {
        switch (kind_) {
        case Kind::Absolute:
            return value_;
        case Kind::AboveBottom:
            return context.minGenY + value_;
        case Kind::BelowTop:
            break;
        }
        return context.minGenY + context.genDepth - 1 - value_;
    }

    friend bool operator==(const VerticalAnchor&, const VerticalAnchor&) = default;

private:
    enum class Kind : std::uint8_t { Absolute, AboveBottom, BelowTop };

    constexpr VerticalAnchor(Kind kind, std::int32_t value) noexcept : kind_(kind), value_(value) {}

    Kind kind_;
    std::int32_t value_;
};

/// Everything the surface rule node types need from outside `server-world`: real noise data, the
/// real per-world seed, biome lookup. An interface rather than a dependency on `server-assets`, the
/// same way the noise routers take a lookup function instead of loading their own parameters.
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

    /// parity: `SurfaceSystem.getSurfaceDepth`.
    [[nodiscard]] virtual std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const = 0;

    /// parity: `NoiseChunk.preliminarySurfaceLevel`, a coarse estimate off the terrain splines that
    /// sits well below the real surface. The window the rules may write in is measured from here,
    /// so using the real height cuts that window short.
    [[nodiscard]] virtual std::int32_t preliminarySurfaceLevel(std::int32_t x, std::int32_t z) const {
        return estimatedSurfaceHeight(x, z);
    }

    /// parity: `SurfaceSystem.getSurfaceSecondary`.
    [[nodiscard]] virtual double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const = 0;

    [[nodiscard]] virtual double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const = 0;

    /// Noise at a scaled, non-integral coordinate, used by the SurfaceSystem extensions.
    [[nodiscard]] virtual double sample2dScaled(std::string_view noiseId, double x, double z) const {
        return sample2d(noiseId, static_cast<std::int32_t>(x), static_cast<std::int32_t>(z));
    }

    [[nodiscard]] virtual double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                          std::int32_t z) const = 0;

    /// A uniform [0, 1) draw from the named positional factory, for the vertical gradient condition.
    [[nodiscard]] virtual float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                                       std::int32_t z) const = 0;

    /// The world's root positional random at a block coordinate.
    [[nodiscard]] virtual random::RandomSource positionalRandom(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// This project runs the surface rules *after* the chunk is filled from the real density router,
    /// so the real generated height is available and used here: a deliberate substitution for
    /// vanilla's estimate, strictly more accurate for the same purpose.
    [[nodiscard]] virtual std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const = 0;

    /// Seed-zoomed block-position biome, matching `BiomeManager.getBiome`.
    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const = 0;

    /// `biomeAt` against one chunk's own cache, so the climate router's flat-cached subtrees resolve
    /// once per quart column instead of once per query.
    [[nodiscard]] virtual const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                     densityfunction::DensityCache* cache) const {
        (void)cache;
        return biomeAt(x, y, z);
    }

    /// Direct noise-biome lookup in quart coordinates, for chunk biome palettes.
    [[nodiscard]] virtual const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                          densityfunction::DensityCache* cache = nullptr) const = 0;

    /// parity: `SurfaceRules.Context.SteepMaterialCondition`.
    [[nodiscard]] virtual bool isSteep(std::int32_t x, std::int32_t z) const = 0;

    /// parity: `SurfaceSystem.getBand`. Only the badlands need it.
    [[nodiscard]] virtual std::int32_t clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const;
};

} // namespace basalt::server::world::terrain::surface

#pragma once

#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace basalt::server::world::terrain::surface {

/// parity: `SurfaceRules.Context`. The per-column and per-block state a driver updates while walking
/// a column top-down, which the condition and rule node types read.
///
/// Vanilla memoizes several fields behind version counters purely for speed; not ported, since
/// surface generation runs once per world rather than per tick.
class SurfaceRuleContext {
public:
    SurfaceRuleContext(const SurfaceRuleEnvironment& environment, WorldGenerationContext worldGenContext)
        : environment_(&environment), worldGenContext_(worldGenContext) {}

    [[nodiscard]] const WorldGenerationContext& worldGenContext() const noexcept { return worldGenContext_; }

    [[nodiscard]] std::int32_t blockX() const noexcept { return blockX_; }

    [[nodiscard]] std::int32_t blockY() const noexcept { return blockY_; }

    [[nodiscard]] std::int32_t blockZ() const noexcept { return blockZ_; }

    [[nodiscard]] std::int32_t surfaceDepth() const noexcept { return surfaceDepth_; }

    [[nodiscard]] std::int32_t waterHeight() const noexcept { return waterHeight_; }

    [[nodiscard]] std::int32_t stoneDepthAbove() const noexcept { return stoneDepthAbove_; }

    [[nodiscard]] std::int32_t stoneDepthBelow() const noexcept { return stoneDepthBelow_; }

    /// parity: `Context.updateXZ`.
    void updateXZ(std::int32_t x, std::int32_t z);

    /// parity: `Context.updateY`.
    void updateY(std::int32_t stoneDepthAbove, std::int32_t stoneDepthBelow, std::int32_t waterHeight,
                 std::int32_t y) noexcept {
        stoneDepthAbove_ = stoneDepthAbove;
        stoneDepthBelow_ = stoneDepthBelow;
        waterHeight_ = waterHeight;
        blockY_ = y;
    }

    [[nodiscard]] double surfaceSecondary() const { return environment_->surfaceSecondaryNoise(blockX_, blockZ_); }

    [[nodiscard]] const std::string& biome() const { return environment_->biomeAt(blockX_, blockY_, blockZ_); }

    [[nodiscard]] bool isHole() const noexcept { return surfaceDepth_ <= 0; }

    [[nodiscard]] bool isSteep() const { return environment_->isSteep(blockX_, blockZ_); }

    /// parity: `Context.getMinSurfaceLevel`. Read at the corners of a 16 by 16 cell and interpolated
    /// across it, so it is a smooth coarse floor rather than the column's own surface.
    [[nodiscard]] std::int32_t minSurfaceLevel() const;

    [[nodiscard]] bool isAbovePreliminarySurface() const { return blockY_ >= minSurfaceLevel(); }

    [[nodiscard]] double sample2d(std::string_view noiseId) const {
        return environment_->sample2d(noiseId, blockX_, blockZ_);
    }

    [[nodiscard]] double sample3d(std::string_view noiseId) const {
        return environment_->sample3d(noiseId, blockX_, blockY_, blockZ_);
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view name) const {
        return environment_->verticalGradientRandom(name, blockX_, blockY_, blockZ_);
    }

    [[nodiscard]] std::int32_t clayBand() const { return environment_->clayBand(blockX_, blockY_, blockZ_); }

private:
    const SurfaceRuleEnvironment* environment_;
    WorldGenerationContext worldGenContext_;
    std::int32_t blockX_ = 0;
    std::int32_t blockZ_ = 0;
    std::int32_t surfaceDepth_ = 0;
    // Cached per column: the estimate depends only on x and z, but the above-surface check runs once
    // per block, and a fresh call would rescan the density function for every one of them.
    std::int32_t estimatedSurfaceHeight_ = 0;
    std::array<std::int32_t, 4> preliminaryCorners_{};
    std::int32_t blockY_ = 0;
    std::int32_t waterHeight_ = std::numeric_limits<std::int32_t>::min();
    std::int32_t stoneDepthAbove_ = 0;
    std::int32_t stoneDepthBelow_ = 0;
};

} // namespace basalt::server::world::terrain::surface

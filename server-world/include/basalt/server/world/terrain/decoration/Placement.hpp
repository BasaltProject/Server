#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// A position a placement pipeline is scattering, chunk-relative on X and Z.
struct DecorationPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const DecorationPos&, const DecorationPos&) = default;
};

/// parity: `PlacementContext`, narrowed to chunk heightmaps.
class PlacementContext {
public:
    explicit PlacementContext(Chunk& chunk) : chunk_(&chunk) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    /// parity: `PlacementContext.getHeight` asks the level, so it sees what decoration already
    /// placed. A snapshot would let every attempt of a count land on bare ground.
    [[nodiscard]] std::int32_t height(std::int32_t localX, std::int32_t localZ,
                                      HeightmapType type = HeightmapType::MotionBlocking) const {
        return chunk_->heightAt(localX, localZ, type);
    }

    /// parity: the `_WG` half of the placement heightmaps, which stops moving after the carvers.
    /// Which of the two a feature names is in its own placed_feature json, not a rule.
    [[nodiscard]] std::int32_t worldgenHeight(std::int32_t localX, std::int32_t localZ, HeightmapType type) const {
        return chunk_->worldgenHeightAt(localX, localZ, type);
    }

private:
    Chunk* chunk_;
};

/// parity: `PlacementModifier`. Each modifier turns one position into zero or more.
using PlacementModifier =
    std::function<std::vector<DecorationPos>(const PlacementContext&, random::RandomSource&, const DecorationPos&)>;

/// parity: `PlacementFilter`, which keeps the position or drops it.
using PlacementPredicate = std::function<bool(const PlacementContext&, random::RandomSource&, const DecorationPos&)>;

[[nodiscard]] PlacementModifier filter(PlacementPredicate predicate);

/// parity: `CountPlacement`.
[[nodiscard]] PlacementModifier countPlacement(IntProvider count);
[[nodiscard]] PlacementModifier countPlacement(std::int32_t count);

/// parity: `InSquarePlacement`, scattering X and Z uniformly within the 16 by 16 chunk.
[[nodiscard]] PlacementModifier inSquarePlacement();

/// parity: `RandomOffsetPlacement`, the symmetric zero-plateau trapezoid case only, which is what
/// every ported placement uses. The general trapezoid formula is not ported.
[[nodiscard]] PlacementModifier randomOffsetPlacement(std::int32_t xzSpread, std::int32_t ySpread);

/// parity: `RandomOffsetPlacement` built from constant providers, which sample without touching the
/// random. Kept apart from the trapezoid form, which always draws.
[[nodiscard]] PlacementModifier constantOffsetPlacement(std::int32_t xzOffset, std::int32_t yOffset);

/// parity: `EnvironmentScanPlacement`. Walks `directionY` until `target` matches, gives up when a
/// step leaves the world or stops matching `allowed`, and tests `target` once more where it broke.
[[nodiscard]] PlacementModifier
environmentScanPlacement(std::int32_t directionY, std::int32_t maxSteps,
                         std::function<bool(std::int32_t, const DecorationPos&)> target,
                         std::function<bool(std::int32_t, const DecorationPos&)> allowed = nullptr);

/// parity: `HeightRangePlacement` with an inclusive uniform provider.
[[nodiscard]] PlacementModifier uniformHeightRangePlacement(std::int32_t minInclusive, std::int32_t maxInclusive);

/// parity: `SurfaceRelativeThresholdFilter`.
[[nodiscard]] PlacementModifier
surfaceRelativeThresholdFilter(HeightmapType heightmap,
                               std::int32_t minInclusive = std::numeric_limits<std::int32_t>::min(),
                               std::int32_t maxInclusive = std::numeric_limits<std::int32_t>::max());

/// parity: `SurfaceWaterDepthFilter`.
[[nodiscard]] PlacementModifier surfaceWaterDepthFilter(std::int32_t maxWaterDepth);

/// parity: `HeightmapPlacement`.
[[nodiscard]] PlacementModifier heightmapPlacement(HeightmapType type = HeightmapType::MotionBlocking);

/// The `_WG` half: the height as the carvers left it. Which one a feature names is in its json.
[[nodiscard]] PlacementModifier worldgenHeightmapPlacement(HeightmapType type);

/// parity: `NoiseBasedCountPlacement`, using the fixed `Biome.BIOME_INFO_NOISE`.
[[nodiscard]] PlacementModifier noiseBasedCountPlacement(std::int32_t noiseToCountRatio, double noiseFactor,
                                                         double noiseOffset = 0.0);

/// parity: `NoiseThresholdCountPlacement`, backed by that same fixed noise sampled at x/200, z/200.
[[nodiscard]] PlacementModifier noiseThresholdCountPlacement(double noiseLevel, std::int32_t belowNoise,
                                                             std::int32_t aboveNoise);

/// parity: `RarityFilter`.
[[nodiscard]] PlacementModifier rarityFilter(std::int32_t chance);

/// parity: `BlockPredicateFilter`, narrowed to the one predicate the ported features need: the
/// target position, and for double-height plants the block above it, must be air.
[[nodiscard]] PlacementModifier airTargetFilter(bool alsoCheckAbove = false);

/// parity: `PlacementUtils.countExtra`, reduced to its own two-outcome weighted list.
[[nodiscard]] PlacementModifier countExtraPlacement(std::int32_t count, float chance, std::int32_t extra);

/// Records an accepted write outside `chunk` in both the feature-local view and the chunk's deferred
/// pipeline, keeping the first state seen in the neighbouring terrain as the delivery guard.
void setExternalDecorationBlockStateId(
    Chunk& chunk, std::vector<std::pair<DecorationPos, std::int32_t>>& externalWrites,
    const std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>& externalBlockState,
    const DecorationPos& position, std::int32_t stateId);

/// Runs `modifiers` from `origin` and places the feature at every position they hand back.
///
/// Depth-first, one position carried all the way through the remaining modifiers before the next is
/// produced. That is not a detail: vanilla chains these as lazy sequences, so the order in which the
/// modifiers draw from `random` is interleaved this way, and running them stage by stage instead
/// would consume the random in a different order and generate a different world.
void placeFrom(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
               const std::vector<PlacementModifier>& modifiers, const std::function<void(const DecorationPos&)>& place);

} // namespace basalt::server::world::terrain::decoration

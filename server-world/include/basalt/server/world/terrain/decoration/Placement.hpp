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

    /// parity: `PlacementContext.getHeight` asks the level.
    [[nodiscard]] std::int32_t height(std::int32_t localX, std::int32_t localZ,
                                      HeightmapType type = HeightmapType::MotionBlocking) const {
        return chunk_->heightAt(localX, localZ, type);
    }

    /// parity: the `_WG` half of the placement heightmaps.
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

/// parity: `RandomOffsetPlacement`.
[[nodiscard]] PlacementModifier randomOffsetPlacement(std::int32_t xzSpread, std::int32_t ySpread);

/// parity: `RandomOffsetPlacement` built from constant providers.
[[nodiscard]] PlacementModifier constantOffsetPlacement(std::int32_t xzOffset, std::int32_t yOffset);

/// parity: `EnvironmentScanPlacement`.
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

[[nodiscard]] PlacementModifier worldgenHeightmapPlacement(HeightmapType type);

/// parity: `NoiseBasedCountPlacement`, using the fixed `Biome.BIOME_INFO_NOISE`.
[[nodiscard]] PlacementModifier noiseBasedCountPlacement(std::int32_t noiseToCountRatio, double noiseFactor,
                                                         double noiseOffset = 0.0);

/// parity: `NoiseThresholdCountPlacement`.
[[nodiscard]] PlacementModifier noiseThresholdCountPlacement(double noiseLevel, std::int32_t belowNoise,
                                                             std::int32_t aboveNoise);

/// parity: `RarityFilter`.
[[nodiscard]] PlacementModifier rarityFilter(std::int32_t chance);

/// parity: `BlockPredicateFilter`.
[[nodiscard]] PlacementModifier airTargetFilter(bool alsoCheckAbove = false);

/// parity: `PlacementUtils.countExtra`, reduced to its own two-outcome weighted list.
[[nodiscard]] PlacementModifier countExtraPlacement(std::int32_t count, float chance, std::int32_t extra);

void setExternalDecorationBlockStateId(
    Chunk& chunk, std::vector<std::pair<DecorationPos, std::int32_t>>& externalWrites,
    const std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>& externalBlockState,
    const DecorationPos& position, std::int32_t stateId);

void placeFrom(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
               const std::vector<PlacementModifier>& modifiers, const std::function<void(const DecorationPos&)>& place);

} // namespace basalt::server::world::terrain::decoration

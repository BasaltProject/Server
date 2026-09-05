#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::frozenocean {

struct DirectionOffset {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

inline constexpr std::array<DirectionOffset, 6> kAllDirections{DirectionOffset{0, -1, 0}, DirectionOffset{0, 1, 0},
                                                               DirectionOffset{0, 0, -1}, DirectionOffset{0, 0, 1},
                                                               DirectionOffset{-1, 0, 0}, DirectionOffset{1, 0, 0}};

inline constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] inline DecorationPos offsetBy(const DecorationPos& position, std::int32_t x, std::int32_t y,
                                            std::int32_t z) {
    return DecorationPos{.x = position.x + x, .y = position.y + y, .z = position.z + z};
}

[[nodiscard]] inline DecorationPos offsetBy(const DecorationPos& position, const DirectionOffset& direction) {
    return offsetBy(position, direction.x, direction.y, direction.z);
}

[[nodiscard]] inline DecorationPos up(const DecorationPos& position) { return offsetBy(position, 0, 1, 0); }

[[nodiscard]] inline DecorationPos down(const DecorationPos& position) { return offsetBy(position, 0, -1, 0); }

[[nodiscard]] bool isFrozenWater(std::int32_t stateId);

/// Small feature-region overlay. An iceberg can extend outside its origin chunk, and retaining those
/// writes in memory is what preserves its shape and smoothing at the chunk edge. At the flush they
/// all go to the chunk, the ones past its edge as deferred writes: the part of an iceberg that falls
/// in the next chunk is still part of the iceberg.
class ChunkFeatureRegion {
public:
    explicit ChunkFeatureRegion(Chunk& chunk) : chunk_(&chunk) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] std::int32_t get(const DecorationPos& position) const;

    void set(const DecorationPos& position, std::int32_t stateId);

    void flush();

private:
    /// One key per block position, so a lookup does not walk the whole overlay. A big iceberg writes
    /// more than fifteen thousand cells and reads each of them several times over.
    [[nodiscard]] static std::int64_t keyOf(const DecorationPos& position) noexcept;

    Chunk* chunk_;
    /// Kept in the order they were written, which is the order the flush hands them over.
    std::vector<std::pair<DecorationPos, std::int32_t>> changes_;
    std::unordered_map<std::int64_t, std::size_t> byPosition_;
};

namespace icebergFeature {

void place(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& suppliedOrigin,
           std::int32_t mainStateId);

} // namespace icebergFeature

namespace blueIceFeature {

bool place(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& origin);

} // namespace blueIceFeature

} // namespace basalt::server::world::terrain::decoration::frozenocean

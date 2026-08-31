#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <array>
#include <cstdint>
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

/// Small feature-region overlay. An iceberg can extend outside its origin chunk, and retaining
/// those writes in memory is what preserves its shape and smoothing at the chunk edge. Only blocks
/// belonging to this chunk are flushed, because this generator owns no neighbouring chunk.
class ChunkFeatureRegion {
public:
    explicit ChunkFeatureRegion(Chunk& chunk) : chunk_(&chunk) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] std::int32_t get(const DecorationPos& position) const;

    void set(const DecorationPos& position, std::int32_t stateId);

    void flush();

private:
    Chunk* chunk_;
    std::vector<std::pair<DecorationPos, std::int32_t>> changes_;
};

namespace icebergFeature {

void place(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& suppliedOrigin,
           std::int32_t mainStateId);

} // namespace icebergFeature

namespace blueIceFeature {

bool place(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& origin);

} // namespace blueIceFeature

} // namespace basalt::server::world::terrain::decoration::frozenocean

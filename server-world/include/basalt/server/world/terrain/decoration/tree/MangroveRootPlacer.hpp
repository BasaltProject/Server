#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `RootPlacer` plus `MangroveRootPlacer`, the only root placer vanilla has.
class MangroveRootPlacer {
public:
    explicit MangroveRootPlacer(IntProvider trunkOffsetY) : trunkOffsetY_(std::move(trunkOffsetY)) {}

    /// parity: `RootPlacer.getTrunkOrigin`.
    [[nodiscard]] std::int32_t trunkOriginY(random::RandomSource& random, std::int32_t originY) const;

    bool placeRoots(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                    std::int32_t originZ, std::int32_t trunkOriginY) const;

private:
    using RootPos = std::tuple<std::int32_t, std::int32_t, std::int32_t>;

    [[nodiscard]] bool simulateRoots(const Chunk& chunk, random::RandomSource& random, std::int32_t rootX,
                                     std::int32_t rootY, std::int32_t rootZ, HorizontalDirection dir,
                                     std::int32_t originX, std::int32_t originY, std::int32_t originZ,
                                     std::vector<RootPos>& rootPositions, std::int32_t layer) const;

    IntProvider trunkOffsetY_;
};

} // namespace basalt::server::world::terrain::decoration::tree

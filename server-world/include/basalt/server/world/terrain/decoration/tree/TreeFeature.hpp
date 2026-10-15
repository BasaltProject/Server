#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/tree/MangroveRootPlacer.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFoliagePlacers.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTrunkPlacers.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTypes.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

struct BlockPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const BlockPos&, const BlockPos&) = default;
    friend auto operator<=>(const BlockPos&, const BlockPos&) = default;
};

struct TreeDecoratorContext {
    std::vector<BlockPos> logs;
    std::vector<BlockPos> leaves;
    std::vector<BlockPos> roots;
    std::vector<BlockPos> decorations;
};

/// parity: `TreeConfiguration`.
struct TreeConfiguration {
    std::int32_t trunkStateId = 0;
    TrunkPlacer trunkPlacer = TrunkPlacer::straight(0, 0, 0);
    std::int32_t foliageStateId = 0;
    FoliagePlacer foliagePlacer = FoliagePlacer::blob(IntProvider::of(0), IntProvider::of(0), 0);
    FeatureSize minimumSize = FeatureSize::twoLayers(0, 0, 0);
    std::optional<MangroveRootPlacer> rootPlacer;
    std::vector<TreeDecoratorConfiguration> decorators;
    bool ignoreVines = false;
};

/// parity: `TreeFeature.place`, `doPlace` and `getMaxFreeTreeHeight`.
namespace treeFeature {

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
           const TreeConfiguration& config);

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
           const TreeConfiguration& config, random::RandomSource& mossyCarpetRandom);

} // namespace treeFeature

} // namespace basalt::server::world::terrain::decoration::tree

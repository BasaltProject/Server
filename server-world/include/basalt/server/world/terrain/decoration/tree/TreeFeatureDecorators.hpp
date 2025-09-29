#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: the `net.minecraft.world.level.levelgen.feature.treedecorators` shapes, each driven from
/// what the tree recorded rather than from the level.
namespace treeDecorators {

/// Runs one decorator. `mossyCarpetRandom` is the region random the pale moss carpet draws from.
void decorate(Chunk& chunk, random::RandomSource& random, random::RandomSource& mossyCarpetRandom,
              const TreeDecoratorContext& context, const TreeDecoratorConfiguration& decorator);

/// parity: `TreeDecorator`'s shuffle, which is `Util.shuffle`, not `Collections.shuffle`.
void shuffle(std::vector<BlockPos>& values, random::RandomSource& random);

} // namespace treeDecorators

} // namespace basalt::server::world::terrain::decoration::tree

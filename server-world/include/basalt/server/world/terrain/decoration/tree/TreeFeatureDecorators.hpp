#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: the `net.minecraft.world.level.levelgen.feature.treedecorators` shapes.
namespace treeDecorators {

void decorate(Chunk& chunk, random::RandomSource& random, random::RandomSource& mossyCarpetRandom,
              const TreeDecoratorContext& context, const TreeDecoratorConfiguration& decorator);

/// parity: `ShelfMushroomDecorator`.
void decorateShelfMushroom(Chunk& chunk, random::RandomSource& random, const std::vector<BlockPos>& logs,
                           float probability);

/// parity: `TreeDecorator`'s shuffle, which is `Util.shuffle`, not `Collections.shuffle`.
void shuffle(std::vector<BlockPos>& values, random::RandomSource& random);

} // namespace treeDecorators

} // namespace basalt::server::world::terrain::decoration::tree

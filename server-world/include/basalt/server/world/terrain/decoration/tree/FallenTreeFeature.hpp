#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `FallenTreeConfiguration`.
struct FallenTreeConfiguration {
    std::int32_t logStateId = 0;
    IntProvider logLength;
    bool stumpVines = false;
};

/// The vanilla fallen-tree configurations the Overworld biome selectors use.
namespace fallenTreeConfigs {

[[nodiscard]] const FallenTreeConfiguration& oak();
[[nodiscard]] const FallenTreeConfiguration& birch();
[[nodiscard]] const FallenTreeConfiguration& superBirch();
[[nodiscard]] const FallenTreeConfiguration& spruce();
[[nodiscard]] const FallenTreeConfiguration& jungle();

} // namespace fallenTreeConfigs

/// parity: `FallenTreeFeature` shape, validation, stump vines and attached mushrooms.
namespace fallenTreeFeature {

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
           const FallenTreeConfiguration& config);

} // namespace fallenTreeFeature

} // namespace basalt::server::world::terrain::decoration::tree

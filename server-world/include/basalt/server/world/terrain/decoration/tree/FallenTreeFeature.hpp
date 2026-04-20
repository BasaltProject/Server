#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `FallenTreeConfiguration`.
struct FallenTreeConfiguration {
    std::int32_t logStateId = 0;
    IntProvider logLength;
    bool stumpVines = false;
    bool brownMushroomsOnly = false;
    /// parity: a `shelf_mushroom` log decorator after the attached mushrooms.
    std::optional<float> shelfMushroom;
};

namespace fallenTreeConfigs {

[[nodiscard]] const FallenTreeConfiguration& oak();
[[nodiscard]] const FallenTreeConfiguration& birch();
[[nodiscard]] const FallenTreeConfiguration& superBirch();
[[nodiscard]] const FallenTreeConfiguration& spruce();
[[nodiscard]] const FallenTreeConfiguration& jungle();
[[nodiscard]] const FallenTreeConfiguration& poplar();

} // namespace fallenTreeConfigs

/// parity: `FallenTreeFeature` shape, validation, stump vines and attached mushrooms.
namespace fallenTreeFeature {

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
           const FallenTreeConfiguration& config);

} // namespace fallenTreeFeature

} // namespace basalt::server::world::terrain::decoration::tree

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SaplingBlock.advanceTree`.
namespace saplingGrowth {

inline constexpr std::int32_t kGrowthLight = 9;
inline constexpr std::int32_t kGrowthOdds = 7;

[[nodiscard]] bool isSapling(std::int32_t stateId);

using Flowering = std::function<bool(const wire::common::Identifier& block)>;

/// Asked once a tree is about to be tried; false keeps the sapling as it is.
using MayGrow = std::function<bool()>;

struct PlannedBlock {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t before = 0;
    std::int32_t after = 0;
};

/// A named tree feature placed at the position in a scratch copy: what would change, with the world left alone.
[[nodiscard]] std::optional<std::vector<PlannedBlock>> planTree(WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::string_view tree,
                                                                random::RandomSource& random);

/// parity: `advanceTree`.
[[nodiscard]] std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, random::RandomSource& random,
                                               const Flowering& flowering = {}, const MayGrow& mayGrow = {});

} // namespace saplingGrowth

} // namespace basalt::server::runtime::world

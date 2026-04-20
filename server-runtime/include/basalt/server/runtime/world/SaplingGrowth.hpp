#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SaplingBlock.advanceTree`.
namespace saplingGrowth {

inline constexpr std::int32_t kGrowthLight = 9;
inline constexpr std::int32_t kGrowthOdds = 7;

[[nodiscard]] bool isSapling(std::int32_t stateId);

using Flowering = std::function<bool(const wire::common::Identifier& block)>;

/// parity: `advanceTree`.
[[nodiscard]] std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, random::RandomSource& random,
                                               const Flowering& flowering = {});

} // namespace saplingGrowth

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `PotentSulfurBlock.validBlockState` and `PotentSulfurBlockEntity.SERVER_WAITING_COUNTDOWN_TICKER`.
namespace geysers {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `PotentSulfurBlock.validBlockState`.
[[nodiscard]] std::optional<std::int32_t> validState(WorldState& world, std::int64_t gameTime, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `block_ticks`.
[[nodiscard]] std::optional<BlockChange> countdownTick(WorldState& world, std::int64_t seed, std::int64_t gameTime,
                                                       std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace geysers

} // namespace basalt::server::runtime::world

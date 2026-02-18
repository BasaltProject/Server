#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// Potent sulfur under water: what it turns into, and the clock a periodic geyser keeps.
///
/// parity: `PotentSulfurBlock.validBlockState` for the state, and
/// `PotentSulfurBlockEntity.SERVER_WAITING_COUNTDOWN_TICKER` for the wait between erupting and lying
/// dormant, which is counted down twenty ticks at a time in the block entity.
///
/// What the plume does to whoever swims into it waits for entities.
namespace geysers {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `PotentSulfurBlock.validBlockState`. Answers the state this block should be in, or
/// nothing when it is already in it.
///
[[nodiscard]] std::optional<std::int32_t> validState(WorldState& world, std::int64_t gameTime, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, std::int32_t stateId);

/// One turn of `SERVER_WAITING_COUNTDOWN_TICKER`: nothing on nineteen ticks out of twenty, and on
/// the twentieth it takes one off the wait and flips the geyser when it runs out. The wait lives in
/// the block entity, counted in twenties as the jar keeps it: a turn booked on the chunk would show
/// up in its `block_ticks`, where the jar has none.
[[nodiscard]] std::optional<BlockChange> countdownTick(WorldState& world, std::int64_t seed, std::int64_t gameTime,
                                                       std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace geysers

} // namespace basalt::server::runtime::world

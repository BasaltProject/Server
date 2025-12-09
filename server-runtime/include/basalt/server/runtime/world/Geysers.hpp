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
/// dormant. Vanilla counts that wait down twenty ticks at a time with nothing to show for it; here
/// the turn it lands on is scheduled instead, which the chunk already knows how to keep.
///
/// What the plume does to whoever swims into it waits for entities.
namespace geysers {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `PotentSulfurBlock.validBlockState`. Answers the state this block should be in, or
/// nothing when it is already in it.
///
/// `seed` and `gameTime` are the world's own, which is all the clock below needs of it.
[[nodiscard]] std::optional<std::int32_t> validState(WorldState& world, std::int64_t seed, std::int64_t gameTime,
                                                     std::int32_t x, std::int32_t y, std::int32_t z,
                                                     std::int32_t stateId);

/// parity: `SERVER_WAITING_COUNTDOWN_TICKER`, whose countdown depends on how deep the water is and
/// on a random forked from the seed at this very position, so two geysers side by side keep their
/// own rhythm and the same world always keeps the same one.
void scheduleTurn(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x, std::int32_t y,
                  std::int32_t z, std::string_view state);

/// parity: the flip at the end of that countdown: dormant becomes erupting, and erupting rests.
void tick(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x, std::int32_t y, std::int32_t z,
          std::vector<BlockChange>& changes);

} // namespace geysers

} // namespace basalt::server::runtime::world

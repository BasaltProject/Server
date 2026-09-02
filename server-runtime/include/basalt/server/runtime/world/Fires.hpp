#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Fire burning out and spreading.
///
/// parity: `FireBlock.tick`, whose numbers are all vanilla's: how readily each block catches and
/// burns away comes from the two tables that class builds at bootstrap, and the rest is its own
/// arithmetic over the fire's age.
///
/// What is not here: rain putting fire out asks whether the sky is open above rather than whether
/// this biome has rain at all, since biome precipitation is not modelled yet.
namespace fires {

/// parity: `FireBlock.getFireTickDelay`.
inline constexpr std::int32_t kMinDelay = 30;
inline constexpr std::int32_t kDelaySpread = 10;

/// parity: `FireBlock.MAX_AGE`.
inline constexpr std::int32_t kMaxAge = 15;

/// What a scheduled tick on a fire is filed under.
[[nodiscard]] const wire::common::Identifier& tickType();

[[nodiscard]] std::int32_t tickDelay(random::RandomSource& random);

[[nodiscard]] bool isFire(std::int32_t stateId);

/// Lights a fire where there is room for one, answering whether it caught.
///
/// parity: `FlintAndSteelDispenseItemBehavior` and the same check a player's flint and steel makes:
/// the space has to be empty and the fire has to have something to stand on.
bool ignite(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

/// Asks for the fire at these coordinates to act after the usual delay.
void schedule(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random);

/// One fire's turn: age it, put it out if it has nothing left to burn, burn what is beside it and
/// try to set light to what is near. Answers every block that changed.
///
/// `difficulty` is what `/difficulty` last set, since a harder world spreads fire further.
[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, const WorldWeather& weather, std::string_view difficulty,
                                            std::int32_t x, std::int32_t y, std::int32_t z,
                                            random::RandomSource& random,
                                            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {},
                                            bool hasSkylight = true);

} // namespace fires

} // namespace basalt::server::runtime::world

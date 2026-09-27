#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Fire burning out and spreading.
///
/// parity: `FireBlock.tick`: how readily each block catches and burns away comes from the two tables
/// that class builds at bootstrap, the rest is its own arithmetic over the fire's age.
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

/// Puts fire where a small fireball landed, answering whether it caught.
///
/// parity: `SmallFireball.onHitBlock`, which asks only that the square be empty: unlike `ignite` it
/// makes no survival check, so a fire with nothing under it is written and goes out on its own turn.
bool igniteOnImpact(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay);

/// Asks for the fire at these coordinates to act after the usual delay.
void schedule(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random);

/// What a fire's turn asks of the world beyond the blocks.
struct Surroundings {
    /// parity: `Level.isRainingAt`, biome and all. Unset, rain falls wherever the sky is open.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)> rainingAt;
    /// parity: the `increased_fire_burnout` attribute where the fire is.
    bool increasedBurnout = false;
    /// parity: `ServerLevel.canSpreadFireAround`, the `fire_spread_radius_around_player` rule.
    bool canSpread = true;
    /// parity: the End's `infiniburn` tag, the only one with bedrock in it.
    bool bedrockBurnsForever = false;
};

/// One fire's turn: age it, put it out if it has nothing left to burn, burn what is beside it and
/// try to set light to what is near. Answers every block that changed. `difficulty` is what
/// `/difficulty` last set, since a harder world spreads fire further.
[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, const WorldWeather& weather, std::string_view difficulty,
                                            std::int32_t x, std::int32_t y, std::int32_t z,
                                            random::RandomSource& random,
                                            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {},
                                            bool hasSkylight = true, const Surroundings& surroundings = {});

} // namespace fires

} // namespace basalt::server::runtime::world

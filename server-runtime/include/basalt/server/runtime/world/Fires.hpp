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

/// parity: `FireBlock.tick`.
namespace fires {

inline constexpr std::int32_t kMinDelay = 30;
inline constexpr std::int32_t kDelaySpread = 10;

inline constexpr std::int32_t kMaxAge = 15;

[[nodiscard]] const wire::common::Identifier& tickType();

[[nodiscard]] std::int32_t tickDelay(const WorldState& world, random::RandomSource& random);

[[nodiscard]] bool isFire(std::int32_t stateId);

/// parity: `FlintAndSteelDispenseItemBehavior`.
bool ignite(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

/// parity: `SmallFireball.onHitBlock` asks only that the square be empty.
bool igniteOnImpact(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay);

void schedule(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random);

struct Surroundings {
    /// parity: `Level.isRainingAt`, biome and all. Unset, the biome in the chunk decides.
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)> rainingAt;
    /// parity: the `increased_fire_burnout` attribute where the fire is.
    bool increasedBurnout = false;
    /// parity: `ServerLevel.canSpreadFireAround`.
    bool canSpread = true;
    /// parity: the End's `infiniburn` tag, the only one with bedrock in it.
    bool bedrockBurnsForever = false;
    /// Asked before fire is laid on a block (x, y, z) from the fire at (fromX, fromY, fromZ).
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t fromX, std::int32_t fromY,
                       std::int32_t fromZ)>
        igniteAllowed;
    /// Asked before the fire at (fromX, fromY, fromZ) burns the block at (x, y, z).
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t fromX,
                       std::int32_t fromY, std::int32_t fromZ)>
        burnAllowed;
};

/// `difficulty` is what `/difficulty` last set, since a harder world spreads fire further.
[[nodiscard]] std::vector<BlockChange> tick(WorldState& world, const WorldWeather& weather, std::string_view difficulty,
                                            std::int32_t x, std::int32_t y, std::int32_t z,
                                            random::RandomSource& random,
                                            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {},
                                            bool hasSkylight = true, const Surroundings& surroundings = {});

} // namespace fires

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/WorldRules.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `ServerLevel.tickChunk`.
namespace randomTicks {

inline constexpr std::int32_t kWalkMultiplier = 3;
inline constexpr std::int32_t kWalkAddend = 1013904223;

/// `cause` is `fade` when something is taken away and `grow` otherwise; only the families in this file ask.
using MayChange = std::function<bool(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     std::int32_t from, std::int32_t to, std::string_view cause)>;

/// A struct rather than a world object so the families are testable without one.
struct Context {
    WorldState& world;
    /// Two worlds picking positions from one walk is not vanilla.
    std::string_view worldName;
    std::int64_t seed = 0;
    /// Only the pitcher crop asks, to leave itself room.
    std::int32_t maxY = 319;
    const GameRuleValues& gameRules;
    /// Unset, every plant grows at vanilla's rate.
    const config::GrowthPercents* growth = nullptr;
    const WorldClock& clock;
    /// `peaceful`, `easy`, `normal`; anything else is hard.
    std::string_view difficulty = "normal";
    bool hasSkylight = true;
    /// parity: `FarmlandBlock.randomTick`, which counts rain as water however far the sea is.
    bool raining = false;
    /// parity: `water_evaporates`.
    bool evaporates = false;
    /// parity: the `spawns_portal_piglins` dimension attribute, which is the overworld's.
    bool spawnsPortalPiglins = false;

    std::function<std::int32_t()> allocateEntityId;
    /// Unset, every change goes through, as on a server with no plugins.
    MayChange mayChange;
    saplingGrowth::Flowering flowering;
    std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)> structureAllowed;
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t type)> levelEvent;
    /// Lifts the players standing in the block, which only the session layer can move; the last argument is unused.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t unused)> pushPlayersUp;
    /// parity: `ServerLevel.tickChunk`'s first loop, one call per roll that hit, on the walk the random ticks share.
    std::function<void(std::int32_t x, std::int32_t z, std::vector<BlockChange>& changes)> precipitate;
};

/// parity: `CropBlock.getGrowthSpeed`.
[[nodiscard]] float growthSpeed(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                const wire::common::Identifier& block);

} // namespace randomTicks

/// An object because vanilla keeps one walk per level and the positions depend on where it has got to.
class RandomTicks {
public:
    /// parity: `randomTickSpeed`.
    [[nodiscard]] std::vector<BlockChange> tick(const randomTicks::Context& context,
                                                std::span<const std::array<std::int32_t, 2>> chunks, std::int32_t speed,
                                                random::RandomSource& random);

    [[nodiscard]] std::vector<BlockChange> tickBlock(const randomTicks::Context& context, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, random::RandomSource& random);

private:
    /// parity: `Level.randValue`.
    std::map<std::string, std::int32_t, std::less<>> walks_;

    [[nodiscard]] static std::int32_t nextWalk(std::int32_t& state);
};

} // namespace basalt::server::runtime::world

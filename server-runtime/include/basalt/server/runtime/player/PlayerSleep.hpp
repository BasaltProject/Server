#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/world/Beds.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// What getting into a bed needs beyond the block itself: where the spawn point is written, who may
/// object, and what a bed in the wrong dimension sets off.
struct SleepContext {
    BreakContext blocks;

    /// Writes this player's spawn point at the bed, and says so on their screen.
    std::function<void(PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)> setSpawn;

    /// Whether a plugin lets this player lie down here. Unset means nobody objects.
    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)> bedAllowed;

    /// The bang a bed makes in a dimension that will not have one.
    std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t byEntityId)> explode;
};

/// Puts a player to bed, or says why not. Answers nothing when they are now asleep.
///
/// Shared because both doors sleep in the same beds: the Java session runs it through its own
/// handler and the Bedrock one has no handler at all.
std::optional<world::BedProblem> sleepInBedFor(const SleepContext& context, PlayerSession& player,
                                               world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                               std::int32_t z, std::int32_t clickedState);

/// Puts a bed's occupied flag back down and tells everyone watching. For a sleeper who is already
/// gone, which is the one case where there is no player left to wake.
void freeBedAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Gets a player out of the bed they are in: the block, the pose and everyone watching. Does nothing
/// to a player who was not in one.
void wakeUpFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld* world);

} // namespace basalt::server::runtime::player

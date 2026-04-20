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

struct SleepContext {
    BreakContext blocks;

    std::function<void(PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)> setSpawn;

    std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z)> bedAllowed;

    std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t byEntityId)> explode;
};

std::optional<world::BedProblem> sleepInBedFor(const SleepContext& context, PlayerSession& player,
                                               world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                               std::int32_t z, std::int32_t clickedState);

void freeBedAt(const BreakContext& context, world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

void wakeUpFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld* world);

} // namespace basalt::server::runtime::player

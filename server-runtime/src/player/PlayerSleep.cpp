#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime::player {
namespace {

/// Says why the bed refused, on that player's own screen.
void refuse(PlayerSession& player, const world::BedProblem problem) {
    if (player.connection().sendActionBar) {
        player.connection().sendActionBar(
            wire::text::Component::translatable(std::string{world::translationKeyOf(problem)}));
    }
}

/// Draws this player lying down, or standing up, on every screen in their world including their own.
void showSleeping(const BreakContext& context, const PlayerSession& player, world::LoadedWorld& world,
                  const std::optional<std::array<std::int32_t, 3>>& bed) {
    for (const PlayerSession* watcher : playersIn(context, world.name())) {
        if (watcher->connection().showSleeping) {
            watcher->connection().showSleeping(player.entityId(), bed);
        }
    }
}

} // namespace

std::optional<world::BedProblem> sleepInBedFor(const SleepContext& context, PlayerSession& player,
                                               world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                                               const std::int32_t z, const std::int32_t clickedState) {
    const std::array<std::int32_t, 3> step = world::beds::headOffset(clickedState);
    const std::array<std::int32_t, 3> head = world::beds::isHead(clickedState)
                                                 ? std::array<std::int32_t, 3>{x, y, z}
                                                 : std::array<std::int32_t, 3>{x + step[0], y + step[1], z + step[2]};
    const std::optional<std::int32_t> headState = world.state().blockState(head[0], head[1], head[2]);
    if (!headState.has_value() || !world::beds::isBed(*headState)) {
        return world::BedProblem::NotHere;
    }

    // parity: `BedBlock.useWithoutItem` does not merely refuse in the Nether or the End: it takes
    // the bed out and sets off an explosion of five where it stood.
    if (world::beds::explodesIn(world.declaration().dimension)) {
        for (const world::BlockChange& change : world::beds::destroy(world.state(), head[0], head[1], head[2])) {
            announceBlock(context.blocks, world.name(), change.x, change.y, change.z, change.stateId);
        }
        if (player.connection().sendActionBar) {
            player.connection().sendActionBar(wire::text::Component::translatable("block.minecraft.bed.not_valid"));
        }
        if (context.explode) {
            context.explode(world, head[0] + 0.5, head[1] + 0.5, head[2] + 0.5, player.entityId());
        }
        return world::BedProblem::NotHere;
    }

    // parity: `ServerPlayer.startSleepInBed` sets the spawn point once the bed is reachable and
    // free, and only then asks whether the night allows sleeping.
    if (world::beds::isOccupied(*headState)) {
        refuse(player, world::BedProblem::Occupied);
        return world::BedProblem::Occupied;
    }
    const double dx = player.x - (head[0] + 0.5);
    const double dy = player.y - head[1];
    const double dz = player.z - (head[2] + 0.5);
    if ((dx * dx) + (dy * dy) + (dz * dz) > world::beds::kRange * world::beds::kRange) {
        refuse(player, world::BedProblem::TooFarAway);
        return world::BedProblem::TooFarAway;
    }
    if (context.setSpawn) {
        context.setSpawn(player, head[0], head[1], head[2]);
    }
    if (!world::beds::isNight(world.clock().totalTicks()) && !world.weather().thundering) {
        refuse(player, world::BedProblem::NotPossibleNow);
        return world::BedProblem::NotPossibleNow;
    }

    const std::int32_t occupied = world::beds::withOccupied(*headState, true);
    world.state().setBlockState(head[0], head[1], head[2], occupied);
    announceBlock(context.blocks, world.name(), head[0], head[1], head[2], occupied);
    if (context.bedAllowed && !context.bedAllowed(player, head[0], head[1], head[2])) {
        return world::BedProblem::NotHere;
    }
    player.sleepingBed = head;
    player.sleepTicks = 0;
    player.stats.reset(storage::stats::timeSinceRest());
    // parity: `ServerPlayer.startSleepInBed` raises it on the success branch, so a bed that refused
    // the player grants nothing.
    if (context.blocks.triggers != nullptr) {
        context.blocks.triggers->onSleptInBed(player.name(), player.uuid().toString());
    }
    showSleeping(context.blocks, player, world, head);
    return std::nullopt;
}

void freeBedAt(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
               const std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    if (!state.has_value() || !world::beds::isBed(*state)) {
        return;
    }
    const std::int32_t awake = world::beds::withOccupied(*state, false);
    world.state().setBlockState(x, y, z, awake);
    announceBlock(context, world.name(), x, y, z, awake);
}

void wakeUpFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld* world) {
    if (!player.sleepingBed.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3> bed = *player.sleepingBed;
    player.sleepingBed.reset();
    player.sleepTicks = 0;
    if (world != nullptr) {
        freeBedAt(context, *world, bed[0], bed[1], bed[2]);
    }
    if (world != nullptr) {
        // Nobody was moved into the bed, so nobody moves out.
        showSleeping(context, player, *world, std::nullopt);
    }
}

} // namespace basalt::server::runtime::player

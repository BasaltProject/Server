#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime::player {
namespace {

void refuse(PlayerSession& player, const std::string_view key) {
    if (player.connection().sendActionBar) {
        player.connection().sendActionBar(wire::text::Component::translatable(std::string{key}));
    }
}

/// parity: `Player.freeAt` over `isSuffocating`.
[[nodiscard]] bool suffocatesAt(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x,
                                const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && context.blockTags != nullptr &&
           world::realBlockTags::contains(*context.blockTags, "minecraft:causes_suffocation", *block) &&
           server::registry::blockShapes::isFullCube(*state);
}

/// parity: `StrawBedBlock.destroyBed`.
void destroyStrawBed(const BreakContext& context, world::LoadedWorld& world, const std::array<std::int32_t, 3>& head) {
    for (const PlayerSession* listener : playersIn(context, world.name())) {
        playSoundFor(*listener, "minecraft:block.straw_bed.break_leave", wire::protocol::play::SoundSource::Blocks,
                     head[0] + 0.5, head[1] + 0.5, head[2] + 0.5, 1.0F, 1.0F,
                     context.nextSeed ? context.nextSeed() : 0);
    }
    for (const world::BlockChange& change : world::beds::destroy(world.state(), head[0], head[1], head[2])) {
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
    }
}

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
    const bool straw = world::beds::isStrawBed(*headState);
    const world::beds::Rule rule = world::beds::ruleFor(world.declaration().dimension, straw);

    // parity: `AbstractBedBlock.useWithoutItem`.
    if (rule.destroyOnUse) {
        if (!rule.errorKey.empty()) {
            refuse(player, rule.errorKey);
        }
        if (straw) {
            destroyStrawBed(context.blocks, world, head);
            return world::BedProblem::NotHere;
        }
        for (const world::BlockChange& change : world::beds::destroy(world.state(), head[0], head[1], head[2])) {
            announceBlock(context.blocks, world.name(), change.x, change.y, change.z, change.stateId);
        }
        if (context.explode) {
            context.explode(world, head[0] + 0.5, head[1] + 0.5, head[2] + 0.5, player.entityId());
        }
        return world::BedProblem::NotHere;
    }
    if (world::beds::isOccupied(*headState)) {
        refuse(player, world::translationKeyOf(world::BedProblem::Occupied));
        return world::BedProblem::Occupied;
    }

    // parity: `ServerPlayer.startSleepInBed`.
    const std::array<std::int32_t, 3> foot{head[0] - step[0], head[1] - step[1], head[2] - step[2]};
    if (!world::beds::reachable(player.x, player.y, player.z, head[0], head[1], head[2]) &&
        !world::beds::reachable(player.x, player.y, player.z, foot[0], foot[1], foot[2])) {
        refuse(player, world::translationKeyOf(world::BedProblem::TooFarAway));
        return world::BedProblem::TooFarAway;
    }
    if (suffocatesAt(context.blocks, world, head[0], head[1] + 1, head[2]) ||
        suffocatesAt(context.blocks, world, foot[0], foot[1] + 1, foot[2])) {
        refuse(player, world::translationKeyOf(world::BedProblem::Obstructed));
        return world::BedProblem::Obstructed;
    }
    const bool dark = world::beds::isNight(world.clock().totalTicks()) || world.weather().thundering;
    const bool setsSpawn = world::beds::holds(rule.canSetSpawn, dark);
    if (setsSpawn && context.setSpawn) {
        context.setSpawn(player, head[0], head[1], head[2]);
    }
    if (!world::beds::holds(rule.canSleep, dark)) {
        if (!rule.errorKey.empty()) {
            refuse(player, rule.errorKey);
        }
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
    // parity: `AbstractBedBlock.getSleptInBedStatType`.
    player.stats.increment(storage::stats::custom(straw ? "sleep_in_straw_bed" : "sleep_in_bed"));
    if (setsSpawn && context.blocks.triggers != nullptr) {
        context.blocks.triggers->fire("slept_in_bed", player.name(), player.uuid().toString());
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
    // parity: `AbstractBedBlock.onStopSleeping`, which is where a straw bed goes.
    if (world::beds::ruleFor(world.declaration().dimension, world::beds::isStrawBed(*state)).destroyOnLeave) {
        destroyStrawBed(context, world, {x, y, z});
    }
}

void wakeUpFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld* world) {
    if (!player.sleepingBed.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3> bed = *player.sleepingBed;
    if (context.bedLeft) {
        context.bedLeft(player, bed[0], bed[1], bed[2]);
    }
    player.sleepingBed.reset();
    // parity: `stopSleepInBed(false, ...)`.
    player.sleepTicks = 100;
    if (world != nullptr) {
        freeBedAt(context, *world, bed[0], bed[1], bed[2]);
    }
    if (world != nullptr) {
        showSleeping(context, player, *world, std::nullopt);
    }
}

} // namespace basalt::server::runtime::player

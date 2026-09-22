#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cstdio>
#include <string>

namespace basalt::server::runtime::player {

namespace {

/// Whether the plugin gate refuses this one, with the screen put back when it does.
[[nodiscard]] bool refused(PlayerSession& player, const wire::common::Identifier& item, const BucketGate& allowed) {
    if (!allowed || allowed(player, static_cast<std::int32_t>(player.x), static_cast<std::int32_t>(player.y),
                            static_cast<std::int32_t>(player.z), item.toString(), item.path() == "bucket")) {
        return false;
    }
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
    return true;
}

/// What the fluid rules read while a bucket pours: the world, its gamerules and where it is.
[[nodiscard]] world::fluidFlow::Context flowingIn(const BreakContext& context, world::LoadedWorld& world,
                                                  random::RandomSource& random) {
    return world::fluidFlow::Context{.world = world.state(),
                                     .gameRules = world.gameRules(),
                                     .random = random,
                                     .ultrawarm = world.declaration().dimension == config::WorldDimension::Nether,
                                     .blockTags = context.blockTags};
}

/// What the world owes a bucket that did something: the blocks, their neighbours, the sensor that
/// hears it and the bucket the hand is left holding.
void settle(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
            const wire::common::Identifier& item, const std::int32_t heldSlot, const storage::StoredItemStack& held,
            const world::buckets::Result& result) {
    // parity: `BucketItem` raises `FLUID_PICKUP` where it took the fluid from and `FLUID_PLACE`
    // where it put it, and a sculk sensor hears both. Which of the two it is comes from the item:
    // an empty bucket is the one that picks up.
    if (!result.changes.empty()) {
        const world::FluidChange& at = result.changes.front();
        if (world.state().hasSculkSensorNear(at.x, at.y, at.z, world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world.state(), item.path() == "bucket" ? "fluid_pickup" : "fluid_place", at.x + 0.5,
                                    at.y + 0.5, at.z + 0.5, world.gameTime(), {.entity = player.uuid()});
        }
    }
    for (const world::FluidChange& change : result.changes) {
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
    }
    // parity: `BucketItem.emptyContents` calls `setBlock`, so what was around it hears about it:
    // grass that had a snow layer over it until the water took its place stops wearing snow.
    for (const world::FluidChange& change : result.changes) {
        notifyAround(context, world, change.x, change.y, change.z, std::nullopt);
    }
    if (!result.replacement.has_value()) {
        return;
    }
    player.inventory.set(
        heldSlot,
        storage::StoredItemStack{.id = result.replacement->toString(), .count = 1, .components = held.components});
    // parity: `BucketItem.use` and `Bucketable.bucketMobPickup` both end in `FILLED_BUCKET.trigger`,
    // which is the bucket that came back full and never the empty one.
    if (context.triggers != nullptr && result.replacement->path() != "bucket") {
        context.triggers->onFilledBucket(player.name(), player.uuid().toString(), *result.replacement);
    }
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
}

} // namespace

bool useBucketFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, const std::int32_t heldSlot,
                  const storage::StoredItemStack& held, const float yaw, const float pitch, const BucketGate& allowed) {
    if (!world::buckets::isBucket(item)) {
        return false;
    }
    if (refused(player, item, allowed)) {
        return true;
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const std::optional<world::buckets::Result> result =
        world::buckets::use(flowing, item, player.x, player.y + world::fluids::eyeHeight(player.sneaking), player.z,
                            yaw, pitch, player.gamemode == wire::protocol::play::Gamemode::Creative);
    if (result.has_value()) {
        settle(context, player, world, item, heldSlot, held, *result);
    }
    return true;
}

bool useBucketOnFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& item, const std::int32_t heldSlot,
                    const storage::StoredItemStack& held, const std::int32_t x, const std::int32_t y,
                    const std::int32_t z, const wire::protocol::play::Direction face, const BucketGate& allowed) {
    if (!world::buckets::isBucket(item)) {
        return false;
    }
    if (refused(player, item, allowed)) {
        return true;
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const std::optional<world::buckets::Result> result = world::buckets::useOn(
        flowing, item, x, y, z, face, player.gamemode == wire::protocol::play::Gamemode::Creative);
    if (result.has_value()) {
        settle(context, player, world, item, heldSlot, held, *result);
    }
    return true;
}

} // namespace basalt::server::runtime::player

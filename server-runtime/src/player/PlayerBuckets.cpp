#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cstdio>
#include <string>

namespace basalt::server::runtime::player {

namespace {

[[nodiscard]] world::buckets::Gate gateFor(PlayerSession& player, const wire::common::Identifier& item,
                                           const BucketGate& allowed) {
    if (!allowed) {
        return {};
    }
    return [&player, &item, &allowed](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                      const std::int32_t faceX, const std::int32_t faceY, const std::int32_t faceZ) {
        std::string_view face = "up";
        if (faceY < 0) {
            face = "down";
        } else if (faceZ < 0) {
            face = "north";
        } else if (faceZ > 0) {
            face = "south";
        } else if (faceX < 0) {
            face = "west";
        } else if (faceX > 0) {
            face = "east";
        }
        if (allowed(player, x, y, z, item.toString(), item.path() == "bucket", face)) {
            return true;
        }
        if (player.connection().sendInventoryAll) {
            player.connection().sendInventoryAll();
        }
        return false;
    };
}

[[nodiscard]] world::fluidFlow::Context flowingIn(const BreakContext& context, world::LoadedWorld& world,
                                                  random::RandomSource& random) {
    return world::fluidFlow::Context{.world = world.state(),
                                     .gameRules = world.gameRules(),
                                     .random = random,
                                     .ultrawarm = world.declaration().dimension == config::WorldDimension::Nether,
                                     .blockTags = context.blockTags};
}

void settle(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
            const wire::common::Identifier& item, const std::int32_t heldSlot, const storage::StoredItemStack& held,
            const world::buckets::Result& result) {
    // parity: `BucketItem`.
    if (!result.changes.empty()) {
        const world::FluidChange& at = result.changes.front();
        if (world.state().hasSculkSensorNear(at.x, at.y, at.z, world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world.state(), item.path() == "bucket" ? "fluid_pickup" : "fluid_place", at.x + 0.5,
                                    at.y + 0.5, at.z + 0.5, world.gameTime(), {.entity = player.uuid()});
        }
    }
    // parity: `BucketItem.use`, which counts the bucket it was whether it filled or emptied.
    statAwards::used(player, item.toString());
    for (const world::FluidChange& change : result.changes) {
        announceBlock(context, world.name(), change.x, change.y, change.z, change.stateId);
    }
    // parity: `BucketItem.emptyContents` calls `setBlock`.
    for (const world::FluidChange& change : result.changes) {
        notifyAround(context, world, change.x, change.y, change.z, std::nullopt);
    }
    if (!result.replacement.has_value()) {
        return;
    }
    player.inventory.set(
        heldSlot,
        storage::StoredItemStack{.id = result.replacement->toString(), .count = 1, .components = held.components});
    // parity: `BucketItem.use` and `Bucketable.bucketMobPickup` trigger `FILLED_BUCKET` with the full bucket.
    if (context.triggers != nullptr && result.replacement->path() != "bucket") {
        context.triggers->fire(
            "filled_bucket", player.name(), player.uuid().toString(),
            world::TriggerEvent{
                .items = {{"item", world::TriggerItem{.item = *result.replacement, .components = held.components}}}});
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
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const std::optional<world::buckets::Result> result = world::buckets::use(
        flowing, item, player.x, player.y + world::fluids::eyeHeight(player.sneaking), player.z, yaw, pitch,
        player.gamemode == wire::protocol::play::Gamemode::Creative, gateFor(player, item, allowed));
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
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const std::optional<world::buckets::Result> result =
        world::buckets::useOn(flowing, item, x, y, z, face, player.gamemode == wire::protocol::play::Gamemode::Creative,
                              gateFor(player, item, allowed));
    if (result.has_value()) {
        settle(context, player, world, item, heldSlot, held, *result);
    }
    return true;
}

} // namespace basalt::server::runtime::player

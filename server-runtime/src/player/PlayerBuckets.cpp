#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobBuckets.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cmath>
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
    const bool mobBucket = world::mobBuckets::ofItem(item) != nullptr;
    player.inventory.set(
        heldSlot, storage::StoredItemStack{.id = result.replacement->toString(),
                                           .count = 1,
                                           .components = mobBucket ? wire::nbt::NbtCompound{} : held.components});
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

[[nodiscard]] bool carries(const PlayerSession& player, const storage::StoredItemStack& stack) {
    for (std::int32_t slot = 0; slot <= PlayerInventory::kOffhandSlot; ++slot) {
        const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
        if (held.has_value() && held->id == stack.id && held->components == stack.components) {
            return true;
        }
    }
    return false;
}

/// parity: `MobBucketItem.checkExtraContent` and its `playEmptySound`.
void placeMob(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
              const world::mobBuckets::Kind& kind, const storage::StoredItemStack& held,
              const world::buckets::Result& result) {
    const server::registry::EntityTypeEntry* entry =
        server::registry::entityTypes::of(wire::common::Identifier::vanilla(std::string{kind.entity}));
    if (entry == nullptr || context.registry == nullptr) {
        return;
    }
    const auto [x, y, z] = result.at;
    if (context.blocks != nullptr) {
        std::vector<PlayerSession*> others = playersIn(context, world.name());
        std::erase(others, &player);
        context.blocks->sendEffects(others, x, y, z, server::registry::blocks::air().stateId,
                                    {world::blockEffect::Sound{.name = std::string{kind.emptySound},
                                                               .source = wire::protocol::play::SoundSource::Blocks}});
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const float yaw = entity::enderDragon::wrapDegrees(random.nextFloat() * 360.0F);
    std::unique_ptr<entity::ServerEntity> mob =
        world::mobBuckets::spawnFrom(kind, held.components, context.registry->allocateEntityId(), entry->protocolId,
                                     x + 0.5, static_cast<double>(y), z + 0.5, yaw);
    if (mob == nullptr) {
        return;
    }
    world.state().spawnEntity(std::move(mob));
    heardBySculk(context, world, "entity_place", x, y, z, {.entity = player.uuid()});
}

} // namespace

void fillResult(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t slot,
                const storage::StoredItemStack& held, const storage::StoredItemStack& result,
                const bool limitCreative) {
    const bool creative = player.gamemode == wire::protocol::play::Gamemode::Creative;
    if (creative && limitCreative) {
        if (!carries(player, result)) {
            static_cast<void>(player.inventory.add(result));
        }
    } else if (!creative && held.count <= 1) {
        player.inventory.set(slot, result);
    } else {
        if (!creative) {
            storage::StoredItemStack left = held;
            left.count -= 1;
            player.inventory.set(slot, left);
        }
        if (const std::int32_t leftover = player.inventory.add(result); leftover > 0) {
            if (const auto id = wire::common::Identifier::parse(result.id); id.has_value()) {
                spawnDroppedItem(context, world, *id, leftover, player.x, player.y + 1.32, player.z, 0.0, 0.0, 0.0, 40,
                                 result.components);
            }
        }
    }
    if (player.connection().sendInventoryAll) {
        player.connection().sendInventoryAll();
    }
}

bool useBucketFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, const std::int32_t heldSlot,
                  const storage::StoredItemStack& held, const float yaw, const float pitch, const BucketGate& allowed) {
    const world::mobBuckets::Kind* mob = world::mobBuckets::ofItem(item);
    if (!world::buckets::isBucket(item) && mob == nullptr) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const bool creative = player.gamemode == wire::protocol::play::Gamemode::Creative;
    const double eyeY = player.y + world::fluids::eyeHeight(player.sneaking);
    const std::optional<world::buckets::Result> result =
        mob != nullptr ? world::buckets::useMob(flowing, mob->water, player.x, eyeY, player.z, yaw, pitch, creative,
                                                mob->water ? gateFor(player, item, allowed) : world::buckets::Gate{})
                       : world::buckets::use(flowing, item, player.x, eyeY, player.z, yaw, pitch, creative,
                                             gateFor(player, item, allowed));
    if (result.has_value()) {
        const storage::StoredItemStack original = held;
        settle(context, player, world, item, heldSlot, original, *result);
        if (mob != nullptr) {
            placeMob(context, player, world, *mob, original, *result);
        }
    }
    return true;
}

bool useBucketOnFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& item, const std::int32_t heldSlot,
                    const storage::StoredItemStack& held, const std::int32_t x, const std::int32_t y,
                    const std::int32_t z, const wire::protocol::play::Direction face, const BucketGate& allowed) {
    const world::mobBuckets::Kind* mob = world::mobBuckets::ofItem(item);
    if (!world::buckets::isBucket(item) && mob == nullptr) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const world::fluidFlow::Context flowing = flowingIn(context, world, random);
    const bool creative = player.gamemode == wire::protocol::play::Gamemode::Creative;
    const std::optional<world::buckets::Result> result =
        mob != nullptr ? world::buckets::useMobOn(flowing, mob->water, x, y, z, face, creative,
                                                  mob->water ? gateFor(player, item, allowed) : world::buckets::Gate{})
                       : world::buckets::useOn(flowing, item, x, y, z, face, creative, gateFor(player, item, allowed));
    if (result.has_value()) {
        const storage::StoredItemStack original = held;
        settle(context, player, world, item, heldSlot, original, *result);
        if (mob != nullptr) {
            placeMob(context, player, world, *mob, original, *result);
        }
    }
    return true;
}

} // namespace basalt::server::runtime::player

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SulfurCubes.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerMobBuckets.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobBuckets.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

#include <cmath>
#include <string>

namespace basalt::server::runtime::player {

bool pickUpMobFor(const BreakContext& context, MobActionSystem* mobs, PlayerSession& player, world::LoadedWorld& world,
                  entity::ServerEntity& mob, const bool mainHand) {
    const world::mobBuckets::Kind* kind = world::mobBuckets::ofEntity(mob.type());
    if (kind == nullptr || mob.removed || mob.dying) {
        return false;
    }
    const std::int32_t slot =
        mainHand ? PlayerInventory::kHotbarStart + player.inventory.heldSlot() : PlayerInventory::kOffhandSlot;
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value()) {
        return false;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
        wire::common::Identifier::parse(held->id);
    if (!item.has_value() || !world::mobBuckets::pickedUpWith(*kind, *item)) {
        return false;
    }
    // parity: `SulfurCube.mobInteract` only reaches `bucketMobPickup` for a grown cube whose fuse is not lit.
    if (!kind->water && (mob.age < 0 || entity::sulfurCubes::primed(mob))) {
        return false;
    }
    storage::StoredItemStack bucket = world::mobBuckets::bucketOf(*kind, mob);
    if (player.events != nullptr) {
        const server::plugins::PluginEvent* event = player.events->ask(
            &server::plugins::pluginEventTypes::playerBucketEntity, [&](server::plugins::PluginEvent& filled) {
                plugins::EventSink::player(filled, player);
                filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
                filled.set("entity_type", mob.type().toString());
                filled.set("hand", std::string{mainHand ? "main_hand" : "off_hand"});
                filled.set("original_item", held->id);
                filled.set("item", bucket.id);
            });
        if (event != nullptr && event->cancelled()) {
            // parity: `Bucketable.bucketMobPickup` resends the hand and the mob after a vetoed pickup.
            if (player.connection().sendInventoryAll) {
                player.connection().sendInventoryAll();
            }
            return true;
        }
        if (event != nullptr) {
            if (const auto* edited = std::get_if<std::string>(&event->get("item"));
                edited != nullptr && *edited != bucket.id && wire::common::Identifier::parse(*edited).has_value()) {
                bucket = storage::StoredItemStack{.id = *edited, .count = 1};
            }
        }
    }
    if (context.blocks != nullptr) {
        context.blocks->sendEffects(
            playersIn(context, world.name()), static_cast<std::int32_t>(std::floor(mob.x)),
            static_cast<std::int32_t>(std::floor(mob.y)), static_cast<std::int32_t>(std::floor(mob.z)),
            server::registry::blocks::air().stateId,
            {world::blockEffect::Sound{.name = std::string{kind->fillSound},
                                       .source = kind->water ? wire::protocol::play::SoundSource::Neutral
                                                             : wire::protocol::play::SoundSource::Hostile}});
    }
    fillResult(context, player, world, slot, *held, bucket, false);
    if (context.triggers != nullptr) {
        if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> filled =
                wire::common::Identifier::parse(bucket.id);
            filled.has_value()) {
            // parity: `Bucketable.bucketMobPickup` triggers `FILLED_BUCKET` with the full bucket.
            context.triggers->fire(
                "filled_bucket", player.name(), player.uuid().toString(),
                world::TriggerEvent{
                    .items = {{"item", world::TriggerItem{.item = *filled, .components = bucket.components}}}});
        }
        context.triggers->fire(
            "player_interacted_with_entity", player.name(), player.uuid().toString(),
            world::TriggerEvent{
                .entities = {{"entity", world::triggerFacts::ofEntity(world, mob)}},
                .items = {{"item", world::TriggerItem{.item = *item, .components = held->components}}}});
    }
    if (mobs != nullptr) {
        mobs->dropLeash(world, mob, true);
    }
    mob.removed = true;
    return true;
}

} // namespace basalt::server::runtime::player

#include "CombatAttackShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerMace.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/BreedingCooldowns.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

namespace basalt::server::runtime {
using namespace combatAttack;

void CombatHandler::handleInteract(const wire::protocol::play::InteractPacket& interact) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || session->gamemode == Gamemode::Spectator || world == nullptr) {
        return;
    }
    ServerEntity* entity = world->state().entity(interact.entityId);
    if (entity == nullptr || entity->removed) {
        return;
    }
    if (session->events != nullptr) {
        const std::string_view hand =
            interact.hand == wire::protocol::play::InteractPacket::kMainHand ? "main_hand" : "off_hand";
        const auto describe = [&](server::plugins::PluginEvent& event) {
            plugins::EventSink::player(event, *session);
            event.set("entity_id", static_cast<std::int64_t>(entity->entityId()));
            event.set("entity_type", entity->type().toString());
            event.set("hand", std::string{hand});
        };
        if (!session->events->allowed(&server::plugins::pluginEventTypes::playerInteractEntityAt,
                                      [&](server::plugins::PluginEvent& event) {
                                          describe(event);
                                          event.set("x", interact.x);
                                          event.set("y", interact.y);
                                          event.set("z", interact.z);
                                      }) ||
            !session->events->allowed(&server::plugins::pluginEventTypes::playerInteractEntity,
                                      [&](server::plugins::PluginEvent& event) {
                                          describe(event);
                                          event.set("action", std::string{"interact"});
                                      })) {
            return;
        }
    }
    if (entity->type().path() == "interaction") {
        recordInteraction(*entity, "interaction", *session, world->gameTime());
        return;
    }
    // parity: `ItemFrame.interact`.
    if (world::itemFrames::isFrame(entity->type()) && useItemFrame(*session, *entity)) {
        return;
    }
    // parity: `Entity.interact`'s leashes.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        const bool shears = inHand.has_value() && inHand->id == "minecraft:shears";
        const MobActionSystem::LeashUse use =
            entity->type().path() == "leash_knot"
                ? mobs->useOnKnot(*world, *session, *entity, shears)
                : mobs->useOnLeashable(*world, *session, *entity, inHand.has_value() && inHand->id == "minecraft:lead",
                                       shears);
        switch (use) {
        case MobActionSystem::LeashUse::Pass:
            // parity: `Entity.interact`, the equipment sheared once no lead was there to cut.
            if (shears && shearEquipment(*session, *entity, slot)) {
                return;
            }
            // parity: `Shearable.shear` from `mobInteract`, the shears worn down once it took.
            if (shears && mobs->readyForShearing(*entity) &&
                mobs->shearMob(*entity, wire::protocol::play::SoundSource::Players)) {
                owner_->blockInteraction().wearHeldItem(1);
                return;
            }
            break;
        case MobActionSystem::LeashUse::Done:
            return;
        case MobActionSystem::LeashUse::DamageShears:
            owner_->blockInteraction().wearHeldItem(1);
            return;
        case MobActionSystem::LeashUse::ConsumeOne:
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `Villager.mobInteract`, which a villager spawn egg skips.
    if (entity->type() == entity::villagers::type()) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
        if (!mainHand || !held.has_value() || held->id != "minecraft:villager_spawn_egg") {
            owner_->trades().openTrades(*entity, mainHand);
            return;
        }
    }
    // parity: `Minecart.interact` seats whoever clicked it.
    if (world::detectorRails::isMinecart(entity->type())) {
        if (entity->type().path() == "minecart") {
            if (!session->sneaking && entity->passengerIds.empty()) {
                mount(*session, *entity);
            }
        } else if (entity->type().path() == "chest_minecart") {
            owner_->menus().openEntityChest(*session, *entity);
        } else if (entity->type().path() == "furnace_minecart" && owner_->options().vehicles != nullptr) {
            // parity: `MinecartFurnace.interact`: a piece of fuel from that hand, spent unless infinite materials.
            const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
            const std::int32_t slot = mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot()
                                               : PlayerInventory::kOffhandSlot;
            const std::optional<storage::StoredItemStack> held = session->inventory.at(slot);
            const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
            const world::realItemTags::TagMembers* itemTags = owner_->options().itemTags;
            const bool fuel = item.has_value() && itemTags != nullptr &&
                              world::realItemTags::contains(*itemTags, "minecraft:furnace_minecart_fuel", *item);
            if (owner_->options().vehicles->fuel(*world, *session, *entity, fuel) &&
                session->gamemode != Gamemode::Creative) {
                storage::StoredItemStack left = *held;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
        }
        return;
    }
    // parity: `AbstractBoat.interact`, a seat for a player not crouching.
    if (entity::boats::isBoat(entity->type())) {
        // parity: `AbstractChestBoat.interact`.
        if (!boardBoat(*session, *entity) && entity::boats::hasChest(entity->type()) &&
            (session->sneaking ||
             static_cast<std::int32_t>(entity->passengerIds.size()) >= entity::boats::maxPassengers(entity->type()))) {
            owner_->menus().openEntityChest(*session, *entity);
        }
        return;
    }
    // parity: `Cushion.interact`: a player not sneaking sits on one nobody is on.
    if (world::cushions::isCushion(entity->type())) {
        if (!session->sneaking && entity->passengerIds.empty() && !session->vehicleId.has_value()) {
            mount(*session, *entity);
            owner_->broadcastSound("minecraft:entity.cushion.sit", entity->x, entity->y, entity->z,
                                   SoundSource::Neutral);
        }
        return;
    }
    if (entity::horses::isHorse(entity->type()) &&
        interactMount(*session, *entity, interact.hand == wire::protocol::play::InteractPacket::kMainHand)) {
        return;
    }
    if (tameOrSit(*world, *session, *entity)) {
        return;
    }
    // parity: `Allay.mobInteract`.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity::allays::isAllay(entity->type())) {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        std::optional<server::world::loot::ItemStack> held;
        if (inHand.has_value() && inHand->count > 0) {
            if (const std::optional<Identifier> id = parsedOrNothing(inHand->id); id.has_value()) {
                held = server::world::loot::ItemStack{
                    .item = *id, .count = inHand->count, .components = inHand->components};
            }
        }
        switch (mobs->useOnAllay(*world, *session, *entity, held, mainHand)) {
        case MobActionSystem::AllayUse::Pass:
            break;
        case MobActionSystem::AllayUse::Done:
            owner_->sendInventory();
            return;
        case MobActionSystem::AllayUse::ConsumeOne:
            // parity: `ItemStack.consume`, which a creative hand keeps.
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `SulfurCube.mobInteract`.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity->type().path() == "sulfur_cube") {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        const std::optional<Identifier> id = inHand.has_value() ? parsedOrNothing(inHand->id) : std::nullopt;
        if (id.has_value() && inHand->count > 0) {
            const server::world::loot::ItemStack held{
                .item = *id, .count = inHand->count, .components = inHand->components};
            switch (mobs->useOnSulfurCube(*entity, held)) {
            case MobActionSystem::SulfurUse::Pass:
                break;
            case MobActionSystem::SulfurUse::Wear:
                if (mainHand) {
                    owner_->blockInteraction().wearHeldItem(1);
                }
                return;
            case MobActionSystem::SulfurUse::Consume:
                if (session->gamemode != Gamemode::Creative) {
                    storage::StoredItemStack left = *inHand;
                    left.count -= 1;
                    session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                    owner_->sendInventorySlot(slot);
                }
                return;
            }
        }
    }
    // parity: `Goat.mobInteract`'s bucket of milk.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity->type().path() == "goat") {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        if (mobs->milkGoat(*world, *session, *entity, slot)) {
            owner_->sendInventory();
            return;
        }
    }
    // parity: `PiglinAi.mobInteract`: a gold ingot to admire and barter.
    if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr && entity->type().path() == "piglin") {
        const bool mainHand = interact.hand == wire::protocol::play::InteractPacket::kMainHand;
        const std::int32_t slot =
            mainHand ? PlayerInventory::kHotbarStart + session->inventory.heldSlot() : PlayerInventory::kOffhandSlot;
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        std::optional<server::world::loot::ItemStack> held;
        if (inHand.has_value() && inHand->count > 0) {
            if (const std::optional<Identifier> id = parsedOrNothing(inHand->id); id.has_value()) {
                held = server::world::loot::ItemStack{
                    .item = *id, .count = inHand->count, .components = inHand->components};
            }
        }
        if (mobs->useOnPiglin(*world, *session, *entity, held)) {
            // parity: `consumeAndReturn`, which a creative hand keeps.
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    // parity: `CopperGolem.mobInteract`: the carried item back, shears, wax and a scrape.
    if (MobActionSystem* mobs = owner_->options().mobs;
        mobs != nullptr && entity::copperGolems::isCopperGolem(entity->type())) {
        const std::int32_t slot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
        const std::optional<storage::StoredItemStack> inHand = session->inventory.at(slot);
        switch (mobs->useOnCopperGolem(*world, *session, *entity,
                                       inHand.has_value() ? parsedOrNothing(inHand->id) : std::nullopt)) {
        case MobActionSystem::GolemUse::Pass:
            break;
        case MobActionSystem::GolemUse::Done:
            return;
        case MobActionSystem::GolemUse::DamageTool:
            owner_->blockInteraction().wearHeldItem(1);
            return;
        case MobActionSystem::GolemUse::ConsumeOne:
            if (session->gamemode != Gamemode::Creative && inHand.has_value()) {
                storage::StoredItemStack left = *inHand;
                left.count -= 1;
                session->inventory.set(slot, left.count <= 0 ? std::nullopt : std::optional{left});
                owner_->sendInventorySlot(slot);
            }
            return;
        }
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session->inventory.at(heldSlot);
    if (!held.has_value()) {
        return;
    }
    const std::optional<Identifier> item = parsedOrNothing(held->id);
    // parity: `ZombieVillager.mobInteract`.
    if (item.has_value() && item->path() == "golden_apple" && entity->type().path() == "zombie_villager") {
        MobActionSystem* mobs = owner_->options().mobs;
        if (mobs != nullptr && mobs->startCure(*world, *entity, session->uuid()) &&
            session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    // parity: `Dolphin.mobInteract`: any fish, a calf grows and a grown one is ready to lead.
    if (item.has_value() && entity->type().path() == "dolphin" && owner_->options().itemTags != nullptr &&
        world::realItemTags::contains(*owner_->options().itemTags, "minecraft:fishes", *item)) {
        owner_->broadcastSound("minecraft:entity.dolphin.eat", entity->x, entity->y, entity->z, SoundSource::Neutral);
        if (entity::breeding::canAgeUp(*entity)) {
            entity::breeding::feedBaby(*entity);
        } else if (MobActionSystem* mobs = owner_->options().mobs; mobs != nullptr) {
            entity->gotFish = true;
            mobs->poseChanged(*entity);
        }
        if (session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    // parity: `Armadillo.mobInteract`.
    if (entity->type().path() == "armadillo") {
        MobActionSystem* mobs = owner_->options().mobs;
        if (item.has_value() && item->path() == "brush" && mobs != nullptr && mobs->brushArmadillo(*world, *entity)) {
            owner_->blockInteraction().wearHeldItem(16);
            return;
        }
        if (entity::armadillos::isScared(*entity)) {
            return;
        }
    }
    // parity: `Tadpole.feed`: the frog food takes a tenth of the wait off, in whole seconds.
    if (item.has_value() && entity->type().path() == "tadpole" && owner_->options().itemTags != nullptr &&
        world::realItemTags::contains(*owner_->options().itemTags, "minecraft:frog_food", *item)) {
        const std::int32_t left = std::max(0, entity::mobConversions::kTadpoleTicks - entity->conversionClock);
        entity->conversionClock += static_cast<std::int32_t>(static_cast<float>(left / 20) * 0.1F) * 20;
        if (session->gamemode != Gamemode::Creative) {
            storage::StoredItemStack rest = *held;
            rest.count = held->count - 1;
            session->inventory.set(heldSlot, rest.count <= 0 ? std::nullopt : std::optional{rest});
            owner_->sendInventorySlot(heldSlot);
        }
        return;
    }
    if (!item.has_value() || !entity::breeding::isFood(entity->type(), *item)) {
        return;
    }
    // parity: `Animal.mobInteract` feeds a grown one into love and a calf into growing faster.
    const std::optional<world::TriggerItem> used = world::triggerFacts::itemOf(held);
    const world::LoadedWorld* herd = owner_->currentWorld();
    const bool waiting = herd != nullptr && herd->state().gameplay().mechanics.animalBreedingCooldownSeconds > 0 &&
                         world::breedingCooldowns::active(session->uuid().toString(), entity->type(), herd->gameTime());
    if (entity::breeding::canFallInLove(*entity) && !waiting) {
        entity::breeding::feed(*entity, session->uuid().toString());
    } else if (entity::breeding::canAgeUp(*entity)) {
        entity::breeding::feedBaby(*entity);
    } else {
        return;
    }
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("player_interacted_with_entity", session->name(), session->uuid().toString(),
                       world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(*world, *entity)}},
                                           .items = {{"item", used.value_or(world::TriggerItem{.item = *item})}}});
    }
    if (session->gamemode != Gamemode::Creative) {
        if (held->count <= 1) {
            session->inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session->inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
}

} // namespace basalt::server::runtime

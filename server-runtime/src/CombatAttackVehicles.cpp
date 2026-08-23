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

bool CombatHandler::useItemFrame(PlayerSession& session, ServerEntity& frame) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    const std::optional<server::world::loot::ItemStack> hand =
        item.has_value() ? std::optional{server::world::loot::ItemStack{
                               .item = *item, .count = held->count, .components = held->components}}
                         : std::nullopt;
    // parity: `ItemFrame.interact` refuses a map that already tracks more than 256 markers.
    if (hand.has_value() && !world::itemFrames::shownIn(frame).has_value() && owner_->options().maps != nullptr &&
        !(frame.sourceNbt.has_value() && frame.sourceNbt->getBoolean("Fixed")) &&
        owner_->options().maps->tooManyMarkers(hand->components, world->settings().rules.tuning.mapMarkerLimit)) {
        return true;
    }
    const world::itemFrames::Touched touched = world::itemFrames::interact(frame, hand);
    if (touched == world::itemFrames::Touched::Nothing) {
        return false;
    }
    broadcastFrame(frame);
    // parity: both halves of `interact` end in `updateNeighbourForOutputSignal`.
    const std::array<std::int32_t, 3> square = world::itemFrames::squareOf(frame);
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), square[0], square[1], square[2],
                                                         world::comparatorReading::of(world->state()));
    if (touched == world::itemFrames::Touched::Took && session.gamemode != Gamemode::Creative && held.has_value()) {
        if (held->count <= 1) {
            session.inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session.inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
    return true;
}

void CombatHandler::broadcastFrame(const ServerEntity& frame) {
    const std::optional<server::world::loot::ItemStack> shown = world::itemFrames::shownIn(frame);
    std::optional<wire::protocol::play::WireItemStack> drawn;
    if (shown.has_value()) {
        const server::registry::Item* entry = server::registry::items::registry().byName(shown->item);
        if (entry != nullptr) {
            drawn = wire::protocol::play::WireItemStack{.itemId = entry->protocolId, .count = shown->count};
        }
    }
    // parity: `DATA_ITEM` is the frame's own first field and `DATA_ROTATION` the one after it.
    const SetEntityDataPacket packet{
        .entityId = frame.entityId(),
        .entries = {SetEntityDataPacket::Entry{.index = SetEntityDataPacket::kItemFrameStackIndex,
                                               .value = SetEntityDataPacket::ItemValue{.stack = drawn}},
                    SetEntityDataPacket::Entry{
                        .index = SetEntityDataPacket::kItemFrameStackIndex + 1,
                        .value = SetEntityDataPacket::IntValue{.value = world::itemFrames::rotationOf(frame)}}}};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool CombatHandler::tameOrSit(const world::LoadedWorld& world, PlayerSession& session, ServerEntity& entity) {
    if (!entity::taming::isTamable(entity.type())) {
        return false;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session.inventory.heldSlot();
    const std::optional<storage::StoredItemStack>& held = session.inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (entity.owner.has_value()) {
        if (*entity.owner != session.uuid().toString()) {
            return true;
        }
        entity.sitting = !entity.sitting;
        entity::ai::navigation::stop(entity);
        broadcastTameFlags(entity);
        return true;
    }
    if (!item.has_value() || !entity::taming::tamedBy(entity.type(), *item)) {
        return false;
    }
    if (session.gamemode != Gamemode::Creative) {
        if (held->count <= 1) {
            session.inventory.set(heldSlot, std::nullopt);
        } else {
            storage::StoredItemStack left = *held;
            left.count = held->count - 1;
            session.inventory.set(heldSlot, left);
        }
        owner_->sendInventorySlot(heldSlot);
    }
    const std::int32_t roll =
        options_.nextDouble ? static_cast<std::int32_t>(options_.nextDouble() * entity::taming::kTameOdds) : 0;
    if (roll != 0) {
        broadcastEntityEvent(entity.entityId(), entity::taming::kEventTamingFailed);
        return true;
    }
    if (hooks_.tameAllowed && !hooks_.tameAllowed(session, entity)) {
        return true;
    }
    entity.owner = session.uuid().toString();
    // parity: `TamableAnimal.tame` ends in `TAME_ANIMAL.trigger`.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("tame_animal", session.name(), session.uuid().toString(),
                       world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(world, entity)}}});
    }
    entity.sitting = true;
    entity.targetId.reset();
    entity::ai::navigation::stop(entity);
    broadcastEntityEvent(entity.entityId(), entity::taming::kEventTamingSucceeded);
    broadcastTameFlags(entity);
    return true;
}

void CombatHandler::mount(PlayerSession& session, ServerEntity& vehicle) {
    if (session.vehicleId == vehicle.entityId()) {
        return;
    }
    if (hooks_.mountAllowed && !hooks_.mountAllowed(session, vehicle)) {
        return;
    }
    // parity: `Entity.startRiding` leaves whatever it was on first.
    if (session.vehicleId.has_value()) {
        dismount(session);
    }
    session.vehicleId = vehicle.entityId();
    vehicle.passengerIds.push_back(session.entityId());
    broadcastPassengers(vehicle);
    // parity: `GameEvent.ENTITY_MOUNT`, from the player at where the vehicle is.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        world::vibrations::emit(world->state(), "entity_mount", vehicle.x, vehicle.y, vehicle.z, world->gameTime(),
                                {.entity = session.uuid()});
    }
    // parity: `Entity.startRiding` ends in `START_RIDING_TRIGGER` for every player now aboard.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire("started_riding", session.name(), session.uuid().toString());
    }
}

void CombatHandler::breakVehicle(PlayerSession& session, ServerEntity& vehicle) {
    world::LoadedWorld* world = owner_->currentWorld();
    VehicleSystem* vehicles = owner_->options().vehicles;
    if (world == nullptr || vehicles == nullptr) {
        return;
    }
    // parity: `Player.attack` against something not alive.
    const std::optional<storage::StoredItemStack>& heldStack = session.inventory.heldItem();
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::DefinitionOf definitionOf = [enchantments](const Identifier& id) {
        return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
    };
    const float strength = player::combat::attackStrengthScale(
        session.ticksSinceSwing, player::playerAttributes::valueOf(session, "minecraft:attack_speed", definitionOf));
    session.ticksSinceSwing = 0;
    const auto base =
        static_cast<float>(player::playerAttributes::valueOf(session, "minecraft:attack_damage", definitionOf));
    const float enchanted = world::weaponDamage::of(heldStack, base, definitionOf);
    const player::combat::Swing swing =
        player::combat::swingOf(strength, base, enchanted, false, session.onGround, session.sprinting);
    if (swing.damage <= 0.0F) {
        return;
    }
    if (swing.knockbackAttack) {
        owner_->broadcastSound(kSoundAttackKnockback, session.x, session.y, session.z, SoundSource::Players);
    }
    const bool landed = vehicles->hurt(*world, vehicle,
                                       VehicleSystem::Hit{.amount = swing.damage,
                                                          .type = world::damageTypes::kPlayerAttack,
                                                          .causeId = session.entityId(),
                                                          .directId = session.entityId(),
                                                          .creativePlayer = session.gamemode == Gamemode::Creative});
    if (!landed) {
        owner_->broadcastSound(kSoundAttackNoDamage, session.x, session.y, session.z, SoundSource::Players);
        return;
    }
    // parity: `causeExtraKnockback`, a push for something that is not alive.
    if (swing.knockbackAttack && !vehicle.removed) {
        const float yaw = session.yaw * (std::numbers::pi_v<float> / 180.0F);
        vehicle.velocityX += static_cast<double>(-wire::common::mth::sin(yaw) * 0.5F);
        vehicle.velocityY += 0.1;
        vehicle.velocityZ += static_cast<double>(wire::common::mth::cos(yaw) * 0.5F);
    }
    owner_->broadcastSound(swing.fullStrength ? kSoundAttackStrong : kSoundAttackWeak, session.x, session.y, session.z,
                           SoundSource::Players);
    if (enchanted - base > 0.0F) {
        broadcastAnimation(vehicle.entityId(), wire::protocol::play::AnimatePacket::kMagicCriticalHit);
    }
    session.food.addExhaustion(session.food.tuning.combat);
}

void CombatHandler::breakCushion(PlayerSession& session, ServerEntity& cushion) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    // parity: `BlockAttachedEntity.hurtServer` kills it and `Cushion.dropItem` follows.
    for (const std::int32_t passenger : cushion.passengerIds) {
        for (PlayerSession* rider : owner_->playersInSameWorld()) {
            if (rider->entityId() == passenger) {
                dismount(*rider);
                break;
            }
        }
    }
    cushion.removed = true;
    world::vibrations::emit(world->state(), "entity_die", cushion.x, cushion.y, cushion.z, world->gameTime(),
                            {.entity = session.uuid()});
    owner_->broadcastSound("minecraft:entity.cushion.break", cushion.x, cushion.y, cushion.z, SoundSource::Neutral);
    if (session.gamemode == Gamemode::Creative || !world->gameRules().get(world::gameRules::kEntityDrops)) {
        return;
    }
    wire::nbt::NbtCompound components;
    if (cushion.sourceNbt.has_value()) {
        if (const wire::nbt::NbtTag* name = cushion.sourceNbt->find("CustomName"); name != nullptr) {
            components.set("minecraft:custom_name", *name);
        }
    }
    owner_->blockInteraction().spawnDroppedItem(world::cushions::itemOf(cushion), 1, cushion.x, cushion.y, cushion.z,
                                                0.0, 0.0, 0.0, 10, std::move(components));
}

void CombatHandler::breakFrame(PlayerSession& session, ServerEntity& frame) {
    world::LoadedWorld* world = owner_->currentWorld();
    // parity: a fixed frame takes nothing from a plain hit.
    if (world == nullptr || (frame.sourceNbt.has_value() && frame.sourceNbt->getBoolean("Fixed"))) {
        return;
    }
    const std::optional<server::world::loot::ItemStack> shown = world::itemFrames::shownIn(frame);
    // parity: `dropItem` puts nothing in the world for a creative player or with `entity_drops` off.
    const bool drops = session.gamemode != Gamemode::Creative && world->gameRules().get(world::gameRules::kEntityDrops);
    if (shown.has_value()) {
        static_cast<void>(frame.sourceNbt->remove("Item"));
        broadcastFrame(frame);
        // parity: `dropItem` takes the map's marker off unless drops are off.
        if (world->gameRules().get(world::gameRules::kEntityDrops) && owner_->options().maps != nullptr) {
            owner_->options().maps->frameLostMap(shown->components, frame);
        }
        if (drops) {
            owner_->blockInteraction().spawnDroppedItem(shown->item, shown->count, frame.x, frame.y, frame.z, 0.0, 0.0,
                                                        0.0, kDefaultPickupDelay, shown->components);
        }
    } else {
        if (hooks_.hangingBreakAllowed && !hooks_.hangingBreakAllowed(session, frame)) {
            return;
        }
        frame.removed = true;
        if (drops) {
            owner_->blockInteraction().spawnDroppedItem(frame.type(), 1, frame.x, frame.y, frame.z);
        }
    }
    // parity: both halves raise `GameEvent.BLOCK_CHANGE`.
    const std::array<std::int32_t, 3> square = world::itemFrames::squareOf(frame);
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), square[0], square[1], square[2],
                                                         world::comparatorReading::of(world->state()));
    return;
}

void CombatHandler::killEntity(ServerEntity& entity) {
    owner_->broadcastSound(entity::entityCombat::deathSound(entity), entity.x, entity.y, entity.z,
                           SoundSource::Hostile);
    broadcastEntityEvent(entity.entityId(), wire::protocol::play::EntityEventPacket::kLivingDeath);
    entity.killed = true;
    entity.dying = true;
}

void CombatHandler::broadcastEntityEvent(std::int32_t entityId, std::int32_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = entityId,
                                                         .eventId = static_cast<std::int8_t>(event)};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastDamageEvent(const ServerEntity& entity, const Identifier& damageType,
                                         std::int32_t causeEntityId) {
    const std::vector<registry::vanillaRegistryData::Entry>& types = registry::vanillaRegistryData::damageTypes();
    std::int32_t registryId = -1;
    for (std::size_t index = 0; index < types.size(); index++) {
        if (types[index].first == damageType.path()) {
            registryId = static_cast<std::int32_t>(index);
            break;
        }
    }
    if (registryId < 0) {
        return;
    }
    const wire::protocol::play::DamageEventPacket packet{.entityId = entity.entityId(),
                                                         .damageType = registryId,
                                                         .causeEntityId = causeEntityId,
                                                         .directEntityId = causeEntityId};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::DamageEventPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastMotion(const ServerEntity& entity) {
    const wire::protocol::play::SetEntityMotionPacket packet{.entityId = entity.entityId(),
                                                             .velocityX = entity.velocityX,
                                                             .velocityY = entity.velocityY,
                                                             .velocityZ = entity.velocityZ};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetEntityMotionPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastTameFlags(const ServerEntity& entity) {
    const SetEntityDataPacket packet{.entityId = entity.entityId(),
                                     .entries = {SetEntityDataPacket::Entry{
                                         .index = static_cast<std::uint8_t>(entity::taming::kFlagsIndex),
                                         .value = SetEntityDataPacket::ByteValue{
                                             .value = static_cast<std::int8_t>(entity::taming::flagsOf(entity))}}}};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastPassengers(const ServerEntity& vehicle) {
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = vehicle.entityId(),
                                                           .passengerIds = vehicle.passengerIds};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPassengers) {
            player->connection().sendPassengers(vehicle.entityId(), vehicle.passengerIds);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastAnimation(std::int32_t entityId, std::int32_t animation) {
    const wire::protocol::play::AnimatePacket packet{.entityId = entityId,
                                                     .action = static_cast<std::uint8_t>(animation)};
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::AnimatePacket::kId,
                                                [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CombatHandler::broadcastParticle(std::string_view particle, double x, double y, double z, std::int32_t count) {
    const server::registry::ParticleType* type =
        server::registry::particleTypes::of(Identifier::vanilla(std::string{particle}));
    if (type == nullptr) {
        return;
    }
    const wire::protocol::play::LevelParticlesPacket packet{
        .particleTypeId = type->protocolId, .x = x, .y = y, .z = z, .count = count};
    const std::string name = type->id.toString();
    for (const PlayerSession* player : owner_->playersInSameWorld()) {
        player::showParticleFor(*player, name, packet);
    }
}

} // namespace basalt::server::runtime

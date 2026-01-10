#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::RemoveEntitiesPacket;
using wire::protocol::play::RotateHeadPacket;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SetEquipmentPacket;
using wire::protocol::play::SpawnEntityPacket;

/// parity: `ServerEntity.sendChanges`, which sends nothing at all for one that has not moved.
constexpr double kPositionEpsilon = 0.0001;
constexpr double kRotationEpsilon = 0.5;

/// parity: `TextDisplay`'s own text and billboard fields.
constexpr std::uint8_t kTextDisplayTextIndex = 23;
constexpr std::uint8_t kDisplayBillboardIndex = 15;
constexpr std::int8_t kBillboardCenter = 3;

/// The equipment slots a player is drawn wearing, in the order they are read off the inventory.
constexpr std::array<SetEquipmentPacket::SlotType, 6> kEquipmentSlots{
    SetEquipmentPacket::SlotType::MainHand, SetEquipmentPacket::SlotType::OffHand, SetEquipmentPacket::SlotType::Feet,
    SetEquipmentPacket::SlotType::Legs,     SetEquipmentPacket::SlotType::Chest,   SetEquipmentPacket::SlotType::Head};

/// Whether this viewer may see that entity at all. parity: what a plugin hid from them.
bool visibleTo(const ServerEntity& entity, const std::string& viewer) {
    if (!entity.visibleTo.has_value()) {
        return true;
    }
    return std::ranges::find(*entity.visibleTo, viewer) != entity.visibleTo->end();
}

} // namespace

EntityVisibilityTracker::EntityVisibilityTracker(ClientSession& owner) : owner_(&owner) {}

void EntityVisibilityTracker::forgetEverythingDrawn() {
    trackedEntities_.clear();
    trackedPlayers_.clear();
    trackedNpcs_.clear();
    namelessTeamMade_ = false;
    lastNpcLook_.clear();
    trackedEquipment_.clear();
    lastEntityPositions_.clear();
    lastRuntimeEntityMetadata_.clear();
    lastPassengers_.clear();
}

void EntityVisibilityTracker::syncSummonedEntities() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || owner_->currentWorld() == nullptr) {
        return;
    }
    const std::string me = session->uuid().toString();
    const std::vector<ServerEntity*> entities = owner_->world().entities();
    for (ServerEntity* entity : entities) {
        if (entity->removed) {
            continue;
        }
        // An entity somebody else can see and this player cannot: taken away if they were already
        // shown it, and skipped if they were not.
        if (!visibleTo(*entity, me)) {
            if (trackedEntities_.erase(entity->entityId()) > 0) {
                lastEntityPositions_.erase(entity->entityId());
                lastRuntimeEntityMetadata_.erase(entity->entityId());
                lastPassengers_.erase(entity->entityId());
                lastLeashHolders_.erase(entity->entityId());
                owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    RemoveEntitiesPacket{.entityIds = {entity->entityId()}}.write(buffer);
                });
            }
            continue;
        }
        if (trackedEntities_.insert(entity->entityId()).second) {
            if (entity::taming::isTamable(entity->type()) && entity::taming::flagsOf(*entity) != 0) {
                owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    SetEntityDataPacket{
                        .entityId = entity->entityId(),
                        .entries = {SetEntityDataPacket::Entry{
                            .index = static_cast<std::uint8_t>(entity::taming::kFlagsIndex),
                            .value = SetEntityDataPacket::ByteValue{.value = static_cast<std::int8_t>(
                                                                        entity::taming::flagsOf(*entity))}}}}
                        .write(buffer);
                });
            }
            owner_->sendPlayPacket(SpawnEntityPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SpawnEntityPacket{.entityId = entity->entityId(),
                                  .uuid = entity->uuid(),
                                  .entityType = entity->protocolType(),
                                  .x = entity->x,
                                  .y = entity->y,
                                  .z = entity->z,
                                  .velocityX = entity->velocityX,
                                  .velocityY = entity->velocityY,
                                  .velocityZ = entity->velocityZ,
                                  .pitch = entity->pitch,
                                  .yaw = entity->yaw,
                                  .headYaw = entity->yaw,
                                  // A falling block is drawn from this: without it every one is stone.
                                  .objectData = entity->objectData}
                    .write(buffer);
            });
            const entity::RuntimeEntityMetadata live = entity::runtimeEntityMetadata(*entity);
            lastRuntimeEntityMetadata_.insert_or_assign(entity->entityId(), live);
            std::vector<SetEntityDataPacket::Entry> metadata;
            if (entity->sourceNbt.has_value()) {
                metadata = entity::generatedEntityMetadata(entity->type().path(), *entity->sourceNbt);
            }
            for (const SetEntityDataPacket::Entry& change : live.changesFrom(nullptr)) {
                std::erase_if(metadata, [&change](const auto& held) { return held.index == change.index; });
                metadata.push_back(change);
            }
            if (!metadata.empty()) {
                owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    SetEntityDataPacket{.entityId = entity->entityId(), .entries = metadata}.write(buffer);
                });
            }
            if (entity->sourceNbt.has_value()) {
                std::vector<SetEquipmentPacket::Slot> equipment;
                for (const entity::GeneratedEquipment& slot : entity::generatedEntityEquipment(*entity->sourceNbt)) {
                    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(slot.item);
                    const server::registry::Item* item =
                        id.has_value() ? server::registry::items::registry().byName(*id) : nullptr;
                    if (item == nullptr) {
                        continue;
                    }
                    world::RealEnchantments* enchantments = owner_->options().enchantments;
                    equipment.push_back(SetEquipmentPacket::Slot{
                        .type = slot.slot,
                        .stack = wire::protocol::play::WireItemStack{
                            .itemId = item->protocolId,
                            .count = slot.count,
                            .components = item::itemComponentCodecs::toWire(
                                slot.components, [enchantments](const Identifier& name) {
                                    return enchantments == nullptr ? std::nullopt : enchantments->protocolIdOf(name);
                                })}});
                }
                if (!equipment.empty()) {
                    owner_->sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                        SetEquipmentPacket{.entityId = entity->entityId(), .slots = equipment}.write(buffer);
                    });
                }
            }
            if (entity->displayText.has_value()) {
                sendDisplayText(entity->entityId(), *entity->displayText);
            }
            syncLeash(*entity);
            continue;
        }
        const entity::RuntimeEntityMetadata live = entity::runtimeEntityMetadata(*entity);
        const auto previous = lastRuntimeEntityMetadata_.find(entity->entityId());
        const std::vector<SetEntityDataPacket::Entry> changes =
            live.changesFrom(previous == lastRuntimeEntityMetadata_.end() ? nullptr : &previous->second);
        lastRuntimeEntityMetadata_.insert_or_assign(entity->entityId(), live);
        if (!changes.empty()) {
            owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEntityDataPacket{.entityId = entity->entityId(), .entries = changes}.write(buffer);
            });
        }
        syncLeash(*entity);
        const auto last = lastEntityPositions_.find(entity->entityId());
        const bool first = last == lastEntityPositions_.end();
        const bool moved = first || std::abs(last->second[0] - entity->x) > kPositionEpsilon ||
                           std::abs(last->second[1] - entity->y) > kPositionEpsilon ||
                           std::abs(last->second[2] - entity->z) > kPositionEpsilon ||
                           std::abs(last->second[3] - static_cast<double>(entity->yaw)) > kRotationEpsilon ||
                           std::abs(last->second[4] - static_cast<double>(entity->headYaw)) > kRotationEpsilon;
        if (!moved) {
            continue;
        }
        // parity: `ClientboundRotateHeadPacket`, which is the only thing that turns a mob's head.
        if (first || std::abs(last->second[4] - static_cast<double>(entity->headYaw)) > kRotationEpsilon) {
            owner_->sendPlayPacket(RotateHeadPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                RotateHeadPacket{.entityId = entity->entityId(), .headYaw = entity->headYaw}.write(buffer);
            });
        }
        lastEntityPositions_.insert_or_assign(
            entity->entityId(), std::array<double, 5>{entity->x, entity->y, entity->z, static_cast<double>(entity->yaw),
                                                      static_cast<double>(entity->headYaw)});
        // parity: the position *sync*, which carries the velocity so the client can interpolate: a
        // teleport every tick snaps, which is what made a falling block judder down.
        owner_->sendPlayPacket(wire::protocol::play::EntityPositionSyncPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::EntityPositionSyncPacket{.entityId = entity->entityId(),
                                                           .x = entity->x,
                                                           .y = entity->y,
                                                           .z = entity->z,
                                                           .velocityX = entity->velocityX,
                                                           .velocityY = entity->velocityY,
                                                           .velocityZ = entity->velocityZ,
                                                           .yaw = entity->yaw,
                                                           .pitch = entity->pitch,
                                                           .onGround = entity->onGround}
                .write(buffer);
        });
    }
    syncPassengers(entities, me, session->entityId());
    std::vector<std::int32_t> gone;
    for (const std::int32_t entityId : owner_->world().collectRemovedEntities()) {
        if (trackedEntities_.erase(entityId) > 0) {
            gone.push_back(entityId);
            lastEntityPositions_.erase(entityId);
            lastRuntimeEntityMetadata_.erase(entityId);
            lastPassengers_.erase(entityId);
            lastLeashHolders_.erase(entityId);
        }
    }
    if (!gone.empty()) {
        owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            RemoveEntitiesPacket{.entityIds = gone}.write(buffer);
        });
    }
}

void EntityVisibilityTracker::syncPassengers(const std::vector<ServerEntity*>& entities, const std::string& viewer,
                                             std::int32_t viewerEntityId) {
    std::set<std::int32_t> visibleEntityIds;
    for (const ServerEntity* entity : entities) {
        if (!entity->removed && visibleTo(*entity, viewer)) {
            visibleEntityIds.insert(entity->entityId());
        }
    }
    for (const ServerEntity* vehicle : entities) {
        if (!trackedEntities_.contains(vehicle->entityId()) || !visibleEntityIds.contains(vehicle->entityId())) {
            continue;
        }
        std::vector<std::int32_t> passengers;
        for (const std::int32_t passengerId : vehicle->passengerIds) {
            if (passengerId == viewerEntityId || trackedPlayers_.contains(passengerId) ||
                (trackedEntities_.contains(passengerId) && visibleEntityIds.contains(passengerId))) {
                passengers.push_back(passengerId);
            }
        }
        const auto previous = lastPassengers_.find(vehicle->entityId());
        const bool had = previous != lastPassengers_.end();
        if (had && previous->second == passengers) {
            continue;
        }
        if (had || !passengers.empty()) {
            owner_->sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::SetPassengersPacket{.vehicleId = vehicle->entityId(), .passengerIds = passengers}
                    .write(buffer);
            });
        }
        if (passengers.empty()) {
            lastPassengers_.erase(vehicle->entityId());
        } else {
            lastPassengers_.insert_or_assign(vehicle->entityId(), passengers);
        }
    }
    std::erase_if(lastPassengers_, [this, &visibleEntityIds](const auto& entry) {
        return !trackedEntities_.contains(entry.first) || !visibleEntityIds.contains(entry.first);
    });
}

void EntityVisibilityTracker::syncLeash(const ServerEntity& entity) {
    const std::optional<std::int32_t> holder = entity.leashHolderId;
    const auto previous = lastLeashHolders_.find(entity.entityId());
    const bool had = previous != lastLeashHolders_.end();
    if (had && holder.has_value() && previous->second == *holder) {
        return;
    }
    if (!had && !holder.has_value()) {
        return;
    }
    owner_->sendPlayPacket(wire::protocol::play::SetEntityLinkPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SetEntityLinkPacket{
            .sourceId = entity.entityId(),
            .destinationId = holder.value_or(wire::protocol::play::SetEntityLinkPacket::kNoVehicle)}
            .write(buffer);
    });
    if (holder.has_value()) {
        lastLeashHolders_.insert_or_assign(entity.entityId(), *holder);
    } else {
        lastLeashHolders_.erase(entity.entityId());
    }
}

void EntityVisibilityTracker::sendDisplayText(std::int32_t entityId, const std::string& text) {
    owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetEntityDataPacket{
            .entityId = entityId,
            .entries = {SetEntityDataPacket::Entry{
                            .index = kTextDisplayTextIndex,
                            .value = SetEntityDataPacket::TextValue{.value = wire::text::miniMessage::parse(text)}},
                        // Turned to face whoever is reading it. A display left as it spawned is
                        // drawn flat in the world, so from the other side it reads backwards.
                        SetEntityDataPacket::Entry{.index = kDisplayBillboardIndex,
                                                   .value = SetEntityDataPacket::ByteValue{.value = kBillboardCenter}}}}
            .write(buffer);
    });
}

void EntityVisibilityTracker::syncPlayers() {
    PlayerSession* session = owner_->playerSession();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (session == nullptr || registry == nullptr) {
        return;
    }
    std::vector<PlayerSession*> visible;
    for (PlayerSession* other : registry->snapshot()) {
        if (other->uuid() != session->uuid() && other->worldName == session->worldName) {
            visible.push_back(other);
        }
    }
    for (PlayerSession* other : visible) {
        if (!trackedPlayers_.insert(other->entityId()).second) {
            continue;
        }
        // parity: the tab-list entry always precedes the entity. The client matches the two by
        // profile uuid to find the skin, and an entity that arrives first is drawn bald and grey.
        owner_->sendPlayPacket(wire::protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlayerInfoUpdatePacket::initializing(
                {wire::protocol::play::PlayerInfoEntry{.uuid = other->uuid(),
                                                       .name = other->name(),
                                                       .listed = true,
                                                       .gamemode = other->gamemode,
                                                       .properties = other->profileProperties}})
                .write(buffer);
        });
        owner_->sendPlayPacket(SpawnEntityPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SpawnEntityPacket{.entityId = other->entityId(),
                              .uuid = other->uuid(),
                              .entityType = SpawnEntityPacket::kPlayerEntityType,
                              .x = other->x,
                              .y = other->y,
                              .z = other->z,
                              .pitch = other->pitch,
                              .yaw = other->yaw,
                              .headYaw = other->yaw}
                .write(buffer);
        });
        owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetEntityDataPacket::skinParts(other->entityId(), other->skinParts).write(buffer);
        });
        owner_->sendPlayPacket(wire::protocol::play::EntityTeleportPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::EntityTeleportPacket{.entityId = other->entityId(),
                                                       .x = other->x,
                                                       .y = other->y,
                                                       .z = other->z,
                                                       .yaw = other->yaw,
                                                       .pitch = other->pitch,
                                                       .onGround = other->onGround}
                .write(buffer);
        });
        owner_->sendPlayPacket(RotateHeadPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            RotateHeadPacket{.entityId = other->entityId(), .headYaw = other->yaw}.write(buffer);
        });
    }

    for (const PlayerSession* other : visible) {
        syncEquipmentOf(*other);
    }

    std::set<std::int32_t> stillThere;
    for (const PlayerSession* other : visible) {
        stillThere.insert(other->entityId());
    }
    std::vector<std::int32_t> gone;
    for (const std::int32_t entityId : trackedPlayers_) {
        if (!stillThere.contains(entityId)) {
            gone.push_back(entityId);
        }
    }
    if (gone.empty()) {
        return;
    }
    for (const std::int32_t entityId : gone) {
        trackedPlayers_.erase(entityId);
        trackedEquipment_.erase(entityId);
    }
    owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        RemoveEntitiesPacket{.entityIds = gone}.write(buffer);
    });
}

void EntityVisibilityTracker::syncEquipmentOf(const PlayerSession& other) {
    std::vector<std::optional<storage::StoredItemStack>> worn{
        other.inventory.at(PlayerInventory::kHotbarStart + other.inventory.heldSlot()),
        other.inventory.at(PlayerInventory::kOffhandSlot)};
    for (std::int32_t index = 0; index < PlayerInventory::kArmourSlots; index++) {
        worn.push_back(other.inventory.at(PlayerInventory::kArmourStart + index));
    }
    const auto tracked = trackedEquipment_.find(other.entityId());
    if (tracked != trackedEquipment_.end() && tracked->second == worn) {
        return;
    }
    const bool first = tracked == trackedEquipment_.end();
    trackedEquipment_.insert_or_assign(other.entityId(), worn);
    // parity: `ServerEntity.sendPairingData` sends nothing for an entity carrying nothing, so an
    // empty-handed player costs no packet at all.
    if (first && std::ranges::none_of(worn, [](const auto& stack) { return stack.has_value(); })) {
        return;
    }
    std::vector<SetEquipmentPacket::Slot> slots;
    for (std::size_t index = 0; index < kEquipmentSlots.size() && index < worn.size(); index++) {
        slots.push_back(SetEquipmentPacket::Slot{.type = kEquipmentSlots[index],
                                                 .stack = other.inventory.toWireStack(worn[index])});
    }
    owner_->sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetEquipmentPacket{.entityId = other.entityId(), .slots = slots}.write(buffer);
    });
}

bool EntityVisibilityTracker::lookedAt(std::int32_t entityId, float yaw, float pitch) {
    /// parity: the smallest turn worth a packet, which is what keeps a still head free.
    constexpr float kLookStep = 1.0F;
    const auto last = lastNpcLook_.find(entityId);
    if (last != lastNpcLook_.end() && std::abs(last->second.first - yaw) < kLookStep &&
        std::abs(last->second.second - pitch) < kLookStep) {
        return false;
    }
    lastNpcLook_.insert_or_assign(entityId, std::pair{yaw, pitch});
    return true;
}

} // namespace basalt::server::runtime

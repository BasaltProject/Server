#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerTabList.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

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

/// parity: `ServerEntity.sendChanges`.
constexpr double kPositionEpsilon = 0.0001;
constexpr double kRotationEpsilon = 0.5;

/// parity: `TextDisplay`'s own text and billboard fields.
constexpr std::uint8_t kTextDisplayTextIndex = 23;
constexpr std::uint8_t kDisplayBillboardIndex = 15;
constexpr std::int8_t kBillboardCenter = 3;

/// parity: the attributes `Attribute.setSyncable` marks.
[[nodiscard]] bool syncable(std::string_view name) {
    static constexpr std::array<std::string_view, 32> kSynced{"air_drag_modifier",
                                                              "armor",
                                                              "armor_toughness",
                                                              "attack_speed",
                                                              "below_name_distance",
                                                              "block_break_speed",
                                                              "block_interaction_range",
                                                              "bounciness",
                                                              "burning_time",
                                                              "camera_distance",
                                                              "explosion_knockback_resistance",
                                                              "entity_interaction_range",
                                                              "fall_damage_multiplier",
                                                              "flying_speed",
                                                              "friction_modifier",
                                                              "gravity",
                                                              "jump_strength",
                                                              "luck",
                                                              "max_absorption",
                                                              "max_health",
                                                              "mining_efficiency",
                                                              "movement_efficiency",
                                                              "movement_speed",
                                                              "name_tag_distance",
                                                              "oxygen_bonus",
                                                              "safe_fall_distance",
                                                              "scale",
                                                              "sneaking_speed",
                                                              "step_height",
                                                              "submerged_mining_speed",
                                                              "sweeping_damage_ratio",
                                                              "water_movement_efficiency"};
    const std::string_view path = name.starts_with("minecraft:") ? name.substr(10) : name;
    return std::ranges::find(kSynced, path) != kSynced.end();
}

/// parity: `ServerEntity.sendPairingData`.
[[nodiscard]] std::vector<wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot>
pairingAttributes(const ServerEntity& entity) {
    using Packet = wire::protocol::play::UpdateAttributesPacket;
    std::map<std::int32_t, Packet::AttributeSnapshot> snapshots;
    const auto record = [&](std::string_view name) {
        const std::optional<std::int32_t> id = server::registry::attributeIds::protocolId(name);
        if (!syncable(name) || !id.has_value() || snapshots.contains(*id)) {
            return;
        }
        Packet::AttributeSnapshot snapshot{.attribute = *id, .base = entity.attribute(name)};
        if (const auto found = entity.attributeModifiers.find(name); found != entity.attributeModifiers.end()) {
            for (const auto& [key, modifier] : found->second) {
                const auto modifierId = Identifier::parse(modifier.id);
                if (modifierId.has_value()) {
                    snapshot.modifiers.push_back(
                        Packet::Modifier{.id = *modifierId,
                                         .amount = modifier.amount,
                                         .operation = static_cast<Packet::Operation>(modifier.operation)});
                }
            }
        }
        snapshots.emplace(*id, std::move(snapshot));
    };
    for (const auto& [name, base] : entity.attributeValues) {
        record(name);
    }
    for (const auto& [name, modifiers] : entity.attributeModifiers) {
        record(name);
    }
    std::vector<Packet::AttributeSnapshot> out;
    out.reserve(snapshots.size());
    for (auto& [id, snapshot] : snapshots) {
        out.push_back(std::move(snapshot));
    }
    return out;
}

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

/// parity: `ChunkMap.TrackedEntity.updatePlayer`, the horizontal range an entity is drawn within.
bool inTrackingRange(const ServerEntity& entity, double viewerX, double viewerZ, std::int32_t viewDistanceChunks,
                     std::int32_t broadcastPercentage) {
    const server::registry::entityTypeShapes::Shape* shape = server::registry::entityTypeShapes::of(entity.type());
    if (shape == nullptr || shape->trackingRange <= 0) {
        return true;
    }
    // parity: `DedicatedServer.getScaledTrackingDistance`, whole blocks scaled by `entity-broadcast-range-percentage`.
    const std::int32_t scaled = broadcastPercentage * (shape->trackingRange * 16) / 100;
    const double range = std::min(static_cast<double>(scaled), viewDistanceChunks * 16.0);
    const double dx = viewerX - entity.x;
    const double dz = viewerZ - entity.z;
    return (dx * dx) + (dz * dz) <= range * range;
}

} // namespace

EntityVisibilityTracker::EntityVisibilityTracker(ClientSession& owner) : owner_(&owner) {}

void EntityVisibilityTracker::forgetEverythingDrawn() {
    trackedEntities_.clear();
    trackedPlayers_.clear();
    lastPlayerData_.clear();
    trackedNpcs_.clear();
    namelessTeamMade_ = false;
    lastNpcLook_.clear();
    trackedEquipment_.clear();
    lastEntityPositions_.clear();
    lastRuntimeEntityMetadata_.clear();
    sentSourceVersion_.clear();
    lastPassengers_.clear();
    lastMobEquipment_.clear();
    lastDisplayText_.clear();
}

void EntityVisibilityTracker::syncSummonedEntities() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || owner_->currentWorld() == nullptr) {
        return;
    }
    const std::string me = session->uuid().toString();
    const std::vector<ServerEntity*> entities = owner_->world().entities();
    const std::int32_t viewDistance = owner_->chunkStreamer().trackedRadius();
    for (ServerEntity* entity : entities) {
        if (entity->removed) {
            continue;
        }
        if (!visibleTo(*entity, me) || session->extras.hiddenEntities.contains(entity->entityId()) ||
            !inTrackingRange(*entity, session->x, session->z, viewDistance,
                             owner_->options().tuning.entityBroadcastRangePercentage)) {
            if (trackedEntities_.erase(entity->entityId()) > 0) {
                lastEntityPositions_.erase(entity->entityId());
                lastRuntimeEntityMetadata_.erase(entity->entityId());
                sentSourceVersion_.erase(entity->entityId());
                lastPassengers_.erase(entity->entityId());
                lastLeashHolders_.erase(entity->entityId());
                lastMobEquipment_.erase(entity->entityId());
                lastDisplayText_.erase(entity->entityId());
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
            if (entity->lying) {
                // parity: `Cat.IS_LYING`, for a cat already lying when it comes into view.
                owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    SetEntityDataPacket{.entityId = entity->entityId(),
                                        .entries = {SetEntityDataPacket::Entry{
                                            .index = 21, .value = SetEntityDataPacket::BoolValue{.value = true}}}}
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
            sentSourceVersion_.insert_or_assign(entity->entityId(), entity->sourceNbtVersion);
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
            syncMobEquipment(*entity);
            if (const std::vector<wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot> attributes =
                    pairingAttributes(*entity);
                !attributes.empty()) {
                owner_->sendPlayPacket(wire::protocol::play::UpdateAttributesPacket::kId,
                                       [&](wire::io::ByteBuffer& buffer) {
                                           wire::protocol::play::UpdateAttributesPacket{.entityId = entity->entityId(),
                                                                                        .attributes = attributes}
                                               .write(buffer);
                                       });
            }
            if (entity->displayText.has_value()) {
                sendDisplayText(entity->entityId(), *entity->displayText);
                lastDisplayText_.insert_or_assign(entity->entityId(), *entity->displayText);
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
        if (entity->sourceNbt.has_value() && sentSourceVersion_[entity->entityId()] != entity->sourceNbtVersion) {
            sentSourceVersion_[entity->entityId()] = entity->sourceNbtVersion;
            std::vector<SetEntityDataPacket::Entry> saved =
                entity::generatedEntityMetadata(entity->type().path(), *entity->sourceNbt);
            const std::vector<SetEntityDataPacket::Entry> runtime = live.changesFrom(nullptr);
            std::erase_if(saved, [&runtime](const SetEntityDataPacket::Entry& held) {
                return std::ranges::any_of(runtime, [&held](const auto& own) { return own.index == held.index; });
            });
            if (!saved.empty()) {
                owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    SetEntityDataPacket{.entityId = entity->entityId(), .entries = saved}.write(buffer);
                });
            }
        }
        syncMobEquipment(*entity);
        if (entity->displayText.has_value()) {
            std::string& shown = lastDisplayText_[entity->entityId()];
            if (shown != *entity->displayText) {
                shown = *entity->displayText;
                sendDisplayText(entity->entityId(), shown);
            }
        }
        syncLeash(*entity);
        const auto last = lastEntityPositions_.find(entity->entityId());
        const bool first = last == lastEntityPositions_.end();
        const bool moved = first || std::abs(last->second[0] - entity->x) > kPositionEpsilon ||
                           std::abs(last->second[1] - entity->y) > kPositionEpsilon ||
                           std::abs(last->second[2] - entity->z) > kPositionEpsilon ||
                           std::abs(last->second[3] - static_cast<double>(entity->yaw)) > kRotationEpsilon ||
                           std::abs(last->second[4] - static_cast<double>(entity->headYaw)) > kRotationEpsilon;
        // parity: `ServerEntity.handleMinecartPosRot`.
        if (entity->cart != nullptr && entity->cart->experimental && !entity->vehicleId.has_value()) {
            world::LoadedWorld* here = owner_->currentWorld();
            const bool fresh = here != nullptr && entity->cart->lerpStepsTick == here->gameTime();
            wire::protocol::play::ClientboundMoveMinecartPacket packet{.entityId = entity->entityId()};
            if (fresh && !entity->cart->lerpSteps.empty()) {
                for (const entity::minecarts::Step& step : entity->cart->lerpSteps) {
                    packet.steps.push_back({.position = step.position,
                                            .movement = step.movement,
                                            .yRot = step.yaw,
                                            .xRot = step.pitch,
                                            .weight = step.weight});
                }
            } else if (moved) {
                packet.steps.push_back({.position = {entity->x, entity->y, entity->z},
                                        .movement = {entity->velocityX, entity->velocityY, entity->velocityZ},
                                        .yRot = entity->yaw,
                                        .xRot = entity->pitch,
                                        .weight = 1.0F});
            }
            if (!packet.steps.empty()) {
                owner_->sendPlayPacket(wire::protocol::play::ClientboundMoveMinecartPacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
            lastEntityPositions_.insert_or_assign(entity->entityId(),
                                                  std::array<double, 5>{entity->x, entity->y, entity->z,
                                                                        static_cast<double>(entity->yaw),
                                                                        static_cast<double>(entity->headYaw)});
            continue;
        }
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
        owner_->sendPlayPacket(wire::protocol::play::EntityPositionSyncPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::EntityPositionSyncPacket{.entityId = entity->entityId(),
                                                           .x = entity->x,
                                                           .y = entity->y,
                                                           .z = entity->z,
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
            sentSourceVersion_.erase(entityId);
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

void EntityVisibilityTracker::syncMobEquipment(const entity::ServerEntity& entity) {
    std::vector<entity::GeneratedEquipment> worn;
    if (entity.sourceNbt.has_value()) {
        worn = entity::generatedEntityEquipment(*entity.sourceNbt);
    }
    std::vector<entity::GeneratedEquipment>& known = lastMobEquipment_[entity.entityId()];
    const auto same = [](const entity::GeneratedEquipment& left, const entity::GeneratedEquipment& right) {
        return left.slot == right.slot && left.item == right.item && left.count == right.count &&
               left.components == right.components;
    };
    if (std::ranges::equal(known, worn, same)) {
        return;
    }
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    std::vector<SetEquipmentPacket::Slot> slots;
    for (const entity::GeneratedEquipment& before : known) {
        if (std::ranges::none_of(worn,
                                 [&](const entity::GeneratedEquipment& now) { return now.slot == before.slot; })) {
            slots.push_back(SetEquipmentPacket::Slot{.type = before.slot, .stack = std::nullopt});
        }
    }
    for (const entity::GeneratedEquipment& slot : worn) {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(slot.item);
        const server::registry::Item* item = id.has_value() ? server::registry::items::registry().byName(*id) : nullptr;
        if (item == nullptr) {
            continue;
        }
        slots.push_back(SetEquipmentPacket::Slot{
            .type = slot.slot,
            .stack = wire::protocol::play::WireItemStack{.itemId = item->protocolId,
                                                         .count = slot.count,
                                                         .components = item::itemComponentCodecs::toWire(
                                                             slot.components, [enchantments](const Identifier& name) {
                                                                 return enchantments == nullptr
                                                                            ? std::nullopt
                                                                            : enchantments->protocolIdOf(name);
                                                             })}});
    }
    known = std::move(worn);
    if (!slots.empty()) {
        owner_->sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetEquipmentPacket{.entityId = entity.entityId(), .slots = slots}.write(buffer);
        });
    }
}

void EntityVisibilityTracker::sendDisplayText(std::int32_t entityId, const std::string& text) {
    owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetEntityDataPacket{
            .entityId = entityId,
            .entries = {SetEntityDataPacket::Entry{
                            .index = kTextDisplayTextIndex,
                            .value = SetEntityDataPacket::TextValue{.value = wire::text::miniMessage::parse(text)}},
                        // Turned to face the reader: a display left as it spawned reads backwards from the other side.
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
        const bool hidden =
            session->extras.hiddenEntities.contains(other->entityId()) ||
            (other->extras.hiddenByDefault && !session->extras.shownEntities.contains(other->entityId()));
        // A new skin shows by taking the player off the screen for a tick and drawing them again.
        const auto seen = profileSeen_.find(other->entityId());
        const bool restyled = seen != profileSeen_.end() && seen->second != other->extras.profileVersion;
        if (other->uuid() != session->uuid() && other->worldName == session->worldName && !hidden && !restyled) {
            visible.push_back(other);
        }
    }
    for (PlayerSession* other : visible) {
        if (!trackedPlayers_.insert(other->entityId()).second) {
            continue;
        }
        profileSeen_[other->entityId()] = other->extras.profileVersion;
        owner_->sendPlayPacket(wire::protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlayerInfoUpdatePacket::initializing(
                {wire::protocol::play::PlayerInfoEntry{.uuid = other->uuid(),
                                                       .name = other->name(),
                                                       .listed = true,
                                                       .gamemode = other->gamemode,
                                                       .properties = other->profileProperties,
                                                       .chatSession = other->chatSession()}})
                .write(buffer);
        });
        if (player::hasCustomListEntry(*other)) {
            player::sendListEntry(*session, *other);
        }
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
        lastPlayerData_.erase(other->entityId());
        syncPlayerData(*other);
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

    syncPlayerData(*session);
    for (const PlayerSession* other : visible) {
        syncPlayerData(*other);
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
    std::vector<wire::common::Uuid> unlisted;
    for (const PlayerSession* other : registry->snapshot()) {
        if (other->worldName == session->worldName && std::ranges::find(gone, other->entityId()) != gone.end()) {
            unlisted.push_back(other->uuid());
        }
    }
    if (!unlisted.empty()) {
        owner_->sendPlayPacket(wire::protocol::play::PlayerInfoRemovePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlayerInfoRemovePacket{.players = unlisted}.write(buffer);
        });
    }
    for (const std::int32_t entityId : gone) {
        trackedPlayers_.erase(entityId);
        profileSeen_.erase(entityId);
        trackedEquipment_.erase(entityId);
        lastPlayerData_.erase(entityId);
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
    // parity: `ServerEntity.sendPairingData`.
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

void EntityVisibilityTracker::syncPlayerData(const PlayerSession& player) {
    const player::PlayerEntityData now = player::PlayerEntityData::of(player);
    const auto last = lastPlayerData_.find(player.entityId());
    const std::vector<SetEntityDataPacket::Entry> changes =
        now.changesFrom(last == lastPlayerData_.end() ? nullptr : &last->second);
    lastPlayerData_.insert_or_assign(player.entityId(), now);
    if (changes.empty()) {
        return;
    }
    owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetEntityDataPacket{.entityId = player.entityId(), .entries = changes}.write(buffer);
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

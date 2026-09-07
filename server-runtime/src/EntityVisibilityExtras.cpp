#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/EntityVisibilityTracker.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::RemoveEntitiesPacket;
using wire::protocol::play::RotateHeadPacket;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SetEquipmentPacket;
using wire::protocol::play::SpawnEntityPacket;

/// parity: how far a dropped item and a generated one are drawn from.
constexpr double kItemViewRadiusSquared = 4096.0;
constexpr std::int32_t kGeneratedEntityViewRadiusChunks = 4;
constexpr double kGeneratedEntityViewRadiusSquared = 4096.0;

/// How far an NPC turns its head to look at somebody.
constexpr double kNpcLookRange = 16.0;
constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

/// A profile name is capped by the protocol, and an NPC's is made from its id.
constexpr std::size_t kProfileNameLimit = 16;
constexpr std::string_view kNamelessTeam = "basalt_nameless";

/// The entity types world generation emits, by their own protocol id.
const std::map<std::string, std::int32_t, std::less<>>& generatedEntityTypeIds() {
    static const std::map<std::string, std::int32_t, std::less<>> kIds = [] {
        std::map<std::string, std::int32_t, std::less<>> ids;
        for (const std::string_view path : {"armor_stand", "item_frame", "glow_item_frame", "painting"}) {
            const server::registry::EntityTypeEntry* entry =
                server::registry::entityTypes::of(Identifier::vanilla(std::string{path}));
            if (entry != nullptr) {
                ids.emplace(path, entry->protocolId);
            }
        }
        return ids;
    }();
    return kIds;
}

std::string npcProfileName(std::int32_t entityId) {
    const std::string name = "npc_" + std::to_string(entityId);
    return name.size() <= kProfileNameLimit ? name : name.substr(0, kProfileNameLimit);
}

/// parity: `ItemFrameBlock`'s own facing, as the spawn packet's extra data.
std::int32_t frameDirectionData(std::string_view facing) {
    if (facing == "down") {
        return 0;
    }
    if (facing == "up") {
        return 1;
    }
    if (facing == "north") {
        return 2;
    }
    if (facing == "south") {
        return 3;
    }
    if (facing == "west") {
        return 4;
    }
    return 5;
}

} // namespace

void EntityVisibilityTracker::hideNameTagOf(const std::string& member) {
    using wire::protocol::play::SetPlayerTeamPacket;
    if (!namelessTeamMade_) {
        namelessTeamMade_ = true;
        owner_->sendPlayPacket(SetPlayerTeamPacket::kId, [](wire::io::ByteBuffer& buffer) {
            SetPlayerTeamPacket{.teamName = std::string{kNamelessTeam},
                                .method = SetPlayerTeamPacket::kMethodAdd,
                                .parameters =
                                    SetPlayerTeamPacket::Parameters{
                                        .displayName = wire::text::Component::text(std::string{kNamelessTeam}),
                                        .nameTagVisibility = wire::protocol::play::NameTagVisibility::Never}}
                .write(buffer);
        });
    }
    owner_->sendPlayPacket(SetPlayerTeamPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetPlayerTeamPacket{
            .teamName = std::string{kNamelessTeam}, .method = SetPlayerTeamPacket::kMethodJoin, .members = {member}}
            .write(buffer);
    });
}

void EntityVisibilityTracker::syncNpcs() {
    PlayerSession* session = owner_->playerSession();
    const ClientSession::PluginHooks& plugins = owner_->options().plugins;
    if (session == nullptr || !plugins.npcs) {
        return;
    }
    const std::string me = session->uuid().toString();
    std::vector<NpcView> here;
    for (const NpcView& npc : plugins.npcs()) {
        const bool visible =
            !npc.visibleTo.has_value() || std::ranges::find(*npc.visibleTo, me) != npc.visibleTo->end();
        if (npc.world == session->worldName && visible) {
            here.push_back(npc);
        }
    }
    for (const NpcView& npc : here) {
        const auto known = trackedNpcs_.find(npc.entityId);
        if (known != trackedNpcs_.end() && known->second == npc.version) {
            continue;
        }
        if (known != trackedNpcs_.end()) {
            // Something a client was already told changed: taken away and drawn again, which is
            // what the profile requires anyway since a skin cannot be changed in place.
            owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                RemoveEntitiesPacket{.entityIds = {npc.entityId}}.write(buffer);
            });
        }
        trackedNpcs_.insert_or_assign(npc.entityId, npc.version);
        // Before the entity: the tag is hidden by the team this name belongs to, and a tag drawn
        // before the team arrives flashes on screen.
        hideNameTagOf(npcProfileName(npc.entityId));
        std::vector<wire::protocol::login::GameProfileProperty> properties;
        if (npc.skinTexture.has_value()) {
            properties.push_back(wire::protocol::login::GameProfileProperty{
                .name = "textures", .value = *npc.skinTexture, .signature = npc.skinSignature});
        }
        owner_->sendPlayPacket(wire::protocol::play::PlayerInfoUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::PlayerInfoUpdatePacket::initializing(
                {wire::protocol::play::PlayerInfoEntry{.uuid = npc.uuid,
                                                       .name = npcProfileName(npc.entityId),
                                                       .listed = false,
                                                       .gamemode = wire::protocol::play::Gamemode::Survival,
                                                       .properties = properties}})
                .write(buffer);
        });
        owner_->sendPlayPacket(SpawnEntityPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SpawnEntityPacket{.entityId = npc.entityId,
                              .uuid = npc.uuid,
                              .entityType = SpawnEntityPacket::kPlayerEntityType,
                              .x = npc.x,
                              .y = npc.y,
                              .z = npc.z,
                              .pitch = npc.pitch,
                              .yaw = npc.yaw,
                              .headYaw = npc.yaw}
                .write(buffer);
        });
        owner_->sendPlayPacket(RotateHeadPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            RotateHeadPacket{.entityId = npc.entityId, .headYaw = npc.yaw}.write(buffer);
        });
        // Every layer of the skin on, or an NPC is drawn without its jacket and hat.
        owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetEntityDataPacket::skinParts(npc.entityId, player::PlayerSession::kAllSkinParts).write(buffer);
        });
    }
    // Heads turn toward whoever is looking, which is per connection: each player sees the NPC
    // looking at them, and none of it is state the server has to keep.
    for (const NpcView& npc : here) {
        if (!npc.looksAtPlayers) {
            continue;
        }
        const double dx = session->x - npc.x;
        // Eyes to eyes, so a tall NPC does not stare at your feet.
        const double dy = session->y - npc.y;
        const double dz = session->z - npc.z;
        const double flat = (dx * dx) + (dz * dz);
        if (flat > kNpcLookRange * kNpcLookRange) {
            continue;
        }
        const auto yaw = static_cast<float>((std::atan2(dz, dx) * kRadiansToDegrees) - 90.0);
        const auto pitch = static_cast<float>(-std::atan2(dy, std::sqrt(flat)) * kRadiansToDegrees);
        if (!lookedAt(npc.entityId, yaw, pitch)) {
            continue;
        }
        owner_->sendPlayPacket(RotateHeadPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            RotateHeadPacket{.entityId = npc.entityId, .headYaw = yaw}.write(buffer);
        });
        owner_->sendPlayPacket(wire::protocol::play::EntityTeleportPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::EntityTeleportPacket{.entityId = npc.entityId,
                                                       .x = npc.x,
                                                       .y = npc.y,
                                                       .z = npc.z,
                                                       .yaw = yaw,
                                                       .pitch = pitch,
                                                       .onGround = true}
                .write(buffer);
        });
    }

    std::set<std::int32_t> visible;
    for (const NpcView& npc : here) {
        visible.insert(npc.entityId);
    }
    std::vector<std::int32_t> gone;
    for (const auto& [entityId, version] : trackedNpcs_) {
        if (!visible.contains(entityId)) {
            gone.push_back(entityId);
        }
    }
    if (gone.empty()) {
        return;
    }
    for (const std::int32_t entityId : gone) {
        trackedNpcs_.erase(entityId);
    }
    owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        RemoveEntitiesPacket{.entityIds = gone}.write(buffer);
    });
}

void EntityVisibilityTracker::syncItemEntities() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || owner_->currentWorld() == nullptr) {
        return;
    }
    std::set<std::int32_t> visible;
    for (const world::DroppedItem& item : owner_->world().items()) {
        const double dx = item.x - session->x;
        const double dz = item.z - session->z;
        if ((dx * dx) + (dz * dz) > kItemViewRadiusSquared) {
            continue;
        }
        const server::registry::Item* entry = server::registry::items::registry().byName(item.item);
        if (entry == nullptr) {
            continue;
        }
        visible.insert(item.entityId);
        if (trackedItems_.insert(item.entityId).second) {
            trackedItemCounts_.insert_or_assign(item.entityId, item.count);
            owner_->sendPlayPacket(SpawnEntityPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SpawnEntityPacket{.entityId = item.entityId,
                                  .uuid = item.uuid,
                                  .entityType = SpawnEntityPacket::kItemEntityType,
                                  .x = item.x,
                                  .y = item.y,
                                  .z = item.z,
                                  .velocityX = item.velocityX,
                                  .velocityY = item.velocityY,
                                  .velocityZ = item.velocityZ}
                    .write(buffer);
            });
            owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEntityDataPacket::item(
                    item.entityId,
                    wire::protocol::play::WireItemStack{.itemId = entry->protocolId, .count = item.count})
                    .write(buffer);
            });
            continue;
        }
        const auto counted = trackedItemCounts_.find(item.entityId);
        if (item.countChanged && (counted == trackedItemCounts_.end() || counted->second != item.count)) {
            // A merge changed the stack: the client draws the number it was told, so it has to hear
            // the new one or two logs on the floor keep looking like one.
            trackedItemCounts_.insert_or_assign(item.entityId, item.count);
            owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEntityDataPacket::item(
                    item.entityId,
                    wire::protocol::play::WireItemStack{.itemId = entry->protocolId, .count = item.count})
                    .write(buffer);
            });
            continue;
        }
        if (item.resting && settledItems_.insert(item.entityId).second) {
            // One correction when it stops, and nothing while it falls: the client simulates the
            // item from the velocity it was spawned with, and a teleport every tick fights that
            // simulation on a different clock, which shows up as a stutter.
            owner_->sendPlayPacket(wire::protocol::play::EntityTeleportPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::EntityTeleportPacket{.entityId = item.entityId,
                                                           .x = item.x,
                                                           .y = item.y,
                                                           .z = item.z,
                                                           .yaw = 0.0F,
                                                           .pitch = 0.0F,
                                                           .onGround = true}
                    .write(buffer);
            });
        }
    }

    std::vector<std::int32_t> gone;
    for (const std::int32_t entityId : trackedItems_) {
        if (!visible.contains(entityId)) {
            gone.push_back(entityId);
        }
    }
    if (gone.empty()) {
        return;
    }
    for (const std::int32_t entityId : gone) {
        trackedItemCounts_.erase(entityId);
        trackedItems_.erase(entityId);
        settledItems_.erase(entityId);
    }
    owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        RemoveEntitiesPacket{.entityIds = gone}.write(buffer);
    });
}

void EntityVisibilityTracker::syncGeneratedEntities() {
    PlayerSession* session = owner_->playerSession();
    player::PlayerRegistry* registry = owner_->options().playerRegistry;
    if (session == nullptr || registry == nullptr || owner_->currentWorld() == nullptr) {
        return;
    }
    std::set<std::int32_t> visible;
    const std::vector<world::GeneratedWorldEntity> entities = owner_->world().generatedEntities(
        [registry] { return registry->allocateEntityId(); }, static_cast<std::int32_t>(std::floor(session->x)) >> 4,
        static_cast<std::int32_t>(std::floor(session->z)) >> 4, kGeneratedEntityViewRadiusChunks);
    for (const world::GeneratedWorldEntity& entity : entities) {
        const server::world::ChunkGeneratedEntity& descriptor = entity.descriptor;
        const double dx = descriptor.x - session->x;
        const double dz = descriptor.z - session->z;
        if ((dx * dx) + (dz * dz) > kGeneratedEntityViewRadiusSquared) {
            continue;
        }
        const auto typeId = generatedEntityTypeIds().find(descriptor.type.path());
        if (typeId == generatedEntityTypeIds().end()) {
            continue;
        }
        visible.insert(entity.entityId);
        if (!trackedGeneratedEntities_.insert(entity.entityId).second) {
            continue;
        }
        const bool isFrame = descriptor.type.path() == "item_frame";
        owner_->sendPlayPacket(SpawnEntityPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SpawnEntityPacket{.entityId = entity.entityId,
                              .uuid = entity.uuid,
                              .entityType = typeId->second,
                              .x = isFrame ? std::floor(descriptor.x) : descriptor.x,
                              .y = isFrame ? std::floor(descriptor.y) : descriptor.y,
                              .z = isFrame ? std::floor(descriptor.z) : descriptor.z,
                              .pitch = descriptor.pitch,
                              .yaw = descriptor.yaw,
                              .headYaw = descriptor.yaw,
                              .objectData = isFrame ? frameDirectionData(descriptor.data.getString("facing")) : 0}
                .write(buffer);
        });
        if (isFrame && descriptor.data.getString("item") == "minecraft:elytra") {
            const server::registry::Item* elytra =
                server::registry::items::registry().byName(Identifier::vanilla("elytra"));
            if (elytra != nullptr) {
                owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    SetEntityDataPacket::itemFrame(
                        entity.entityId, wire::protocol::play::WireItemStack{.itemId = elytra->protocolId, .count = 1})
                        .write(buffer);
                });
            }
        }
        const std::vector<SetEntityDataPacket::Entry> metadata =
            entity::generatedEntityMetadata(descriptor.type.path(), descriptor.data);
        if (!metadata.empty()) {
            owner_->sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEntityDataPacket{.entityId = entity.entityId, .entries = metadata}.write(buffer);
            });
        }
        std::vector<SetEquipmentPacket::Slot> equipment;
        for (const entity::GeneratedEquipment& slot : entity::generatedEntityEquipment(descriptor.data)) {
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
                    .components =
                        item::itemComponentCodecs::toWire(slot.components, [enchantments](const Identifier& name) {
                            return enchantments == nullptr ? std::nullopt : enchantments->protocolIdOf(name);
                        })}});
        }
        if (!equipment.empty()) {
            owner_->sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEquipmentPacket{.entityId = entity.entityId, .slots = equipment}.write(buffer);
            });
        }
    }

    std::vector<std::int32_t> gone;
    for (const std::int32_t entityId : trackedGeneratedEntities_) {
        if (!visible.contains(entityId)) {
            gone.push_back(entityId);
        }
    }
    if (gone.empty()) {
        return;
    }
    for (const std::int32_t entityId : gone) {
        trackedGeneratedEntities_.erase(entityId);
    }
    owner_->sendPlayPacket(RemoveEntitiesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        RemoveEntitiesPacket{.entityIds = gone}.write(buffer);
    });
}

} // namespace basalt::server::runtime

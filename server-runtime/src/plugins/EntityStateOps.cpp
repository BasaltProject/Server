#include "MobStateTable.hpp"

#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using namespace values;

constexpr std::uint8_t kCrouching = 0x02;
constexpr std::uint8_t kSprinting = 0x08;
constexpr std::uint8_t kSwimming = 0x10;
constexpr std::uint8_t kGliding = 0x80;

template <typename T>
[[nodiscard]] Accessor plain(T ServerEntity::* member) {
    return Accessor{[member](const ServerEntity& entity) -> Value { return toValue(entity.*member); },
                    [member](ServerEntity& entity, const Value& value) {
                        fromValue(entity.*member, value);
                        return true;
                    }};
}

[[nodiscard]] Accessor sharedFlag(std::uint8_t bit) {
    return Accessor{[bit](const ServerEntity& entity) -> Value { return (entity.forcedFlags & bit) != 0; },
                    [bit](ServerEntity& entity, const Value& value) {
                        entity.forcedFlags = static_cast<std::uint8_t>(asBool(value) ? entity.forcedFlags | bit
                                                                                     : entity.forcedFlags & ~bit);
                        if (entity.forcedPose == -1) {
                            entity.forcedPose = -2;
                        }
                        return true;
                    }};
}

[[nodiscard]] MobActionSystem* mobSystem(const Environment& environment) {
    return environment.hooks != nullptr && environment.hooks->mobSystem ? environment.hooks->mobSystem() : nullptr;
}

template <typename Packet>
void sendTo(const Environment& environment, const std::vector<std::string>& only, std::int32_t packetId,
            const Packet& packet) {
    if (environment.world == nullptr || !environment.players) {
        return;
    }
    for (player::PlayerSession* viewer : environment.players()) {
        if (viewer->worldName != environment.world->name() || !viewer->connection().sendPlayPacket) {
            continue;
        }
        if (!only.empty() && std::ranges::find(only, viewer->uuid().toString()) == only.end()) {
            continue;
        }
        viewer->connection().sendPlayPacket(packetId,
                                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

[[nodiscard]] std::vector<std::string> viewersOf(const Arguments& arguments, std::size_t from) {
    std::vector<std::string> out;
    for (std::size_t index = from; index < arguments.size(); ++index) {
        if (const std::optional<std::string> text = asText(arguments[index]); text.has_value() && !text->empty()) {
            out.push_back(*text);
        }
    }
    return out;
}

} // namespace

void addEntityState(Table& table, Operations& operations) {
    table["gliding"] = sharedFlag(kGliding);
    table["swimming"] = sharedFlag(kSwimming);
    table["crouching"] = sharedFlag(kCrouching);
    table["sprinting"] = sharedFlag(kSprinting);
    table["pose"] = Accessor{[](const ServerEntity& entity) -> Value {
                                 return static_cast<std::int64_t>(std::max<std::int8_t>(entity.forcedPose, -1));
                             },
                             [](ServerEntity& entity, const Value& value) {
                                 const std::int64_t pose = asInt(value, -1);
                                 entity.forcedPose =
                                     pose < 0 || pose > 17 ? std::int8_t{-2} : static_cast<std::int8_t>(pose);
                                 return true;
                             }};
    table["visible_by_default"] =
        Accessor{[](const ServerEntity& entity) -> Value { return !entity.visibleTo.has_value(); },
                 [](ServerEntity& entity, const Value& value) {
                     if (asBool(value, true)) {
                         entity.visibleTo.reset();
                     } else {
                         entity.visibleTo = std::vector<std::string>{};
                     }
                     return true;
                 }};
    table["hurt_direction"] = plain(&ServerEntity::hurtDirection);
    table["sound_volume"] = Accessor{
        [](const ServerEntity& entity) -> Value { return static_cast<double>(entity::mobSounds::volume(entity)); },
        nullptr};
    table["move_forward"] = plain(&ServerEntity::strafeForward);
    table["move_sideways"] = plain(&ServerEntity::strafeRight);
    table["move_up"] = plain(&ServerEntity::swimUp);
    table["collidable"] = plain(&ServerEntity::collidable);
    table["spawn_reason"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.spawnReason; }, nullptr};
    table["visual_fire"] = plain(&ServerEntity::visualFire);
    table["freeze_locked"] = plain(&ServerEntity::freezeLocked);
    table["jumping"] = plain(&ServerEntity::jumping);
    table["portal_cooldown"] = plain(&ServerEntity::portalCooldown);
    table["no_action_time"] = plain(&ServerEntity::idleTicks);
    table["no_physics"] = plain(&ServerEntity::noPhysics);
    table["riptiding"] = plain(&ServerEntity::riptiding);
    table["bee_stingers"] = plain(&ServerEntity::stingerCount);
    table["bee_stinger_cooldown"] = plain(&ServerEntity::removeStingerTime);
    table["arrow_cooldown"] = plain(&ServerEntity::removeArrowTime);
    table["max_no_damage_ticks"] =
        Accessor{[](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.maxNoDamageTicks); },
                 [](ServerEntity& entity, const Value& value) {
                     entity.maxNoDamageTicks = static_cast<std::int32_t>(std::max<std::int64_t>(asInt(value, 20), 0));
                     return true;
                 }};
    table["active_slot"] = Accessor{
        [](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.activeSlot); }, nullptr};
    table["active_used"] = plain(&ServerEntity::activeUsed);
    table["active_ticks"] =
        Accessor{[](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.activeTicks); },
                 [](ServerEntity& entity, const Value& value) {
                     entity.activeTicks = static_cast<std::int32_t>(asInt(value));
                     return entity.activeSlot >= 0;
                 }};

    operations["start_using"] = [](const Environment&, ServerEntity& entity,
                                   const Arguments& arguments) -> std::optional<Value> {
        entity.activeSlot = asInt(at(arguments, 0)) == 1 ? 1 : 0;
        entity.activeTicks = static_cast<std::int32_t>(std::max<std::int64_t>(asInt(at(arguments, 1), 72000), 1));
        entity.activeUsed = 0;
        return true;
    };
    operations["stop_using"] = [](const Environment&, ServerEntity& entity, const Arguments&) -> std::optional<Value> {
        const bool was = entity.activeSlot >= 0;
        entity.activeSlot = -1;
        entity.activeTicks = 0;
        entity.activeUsed = 0;
        return was;
    };
    operations["swing_hand"] = [](const Environment& environment, ServerEntity& entity,
                                  const Arguments& arguments) -> std::optional<Value> {
        const wire::protocol::play::SwingAnimationPacket packet{.entityId = entity.entityId(),
                                                                .hand = asInt(at(arguments, 0)) == 1 ? 1 : 0};
        sendTo(environment, {}, wire::protocol::play::SwingAnimationPacket::kId, packet);
        return true;
    };
    operations["hurt_animation"] = [](const Environment& environment, ServerEntity& entity,
                                      const Arguments& arguments) -> std::optional<Value> {
        const wire::protocol::play::HurtAnimationPacket packet{.entityId = entity.entityId(),
                                                               .yaw = static_cast<float>(asReal(at(arguments, 0)))};
        sendTo(environment, viewersOf(arguments, 1), wire::protocol::play::HurtAnimationPacket::kId, packet);
        return true;
    };
    operations["entity_event"] = [](const Environment& environment, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        const wire::protocol::play::EntityEventPacket packet{
            .entityId = entity.entityId(), .eventId = static_cast<std::int8_t>(asInt(at(arguments, 0)))};
        sendTo(environment, viewersOf(arguments, 1), wire::protocol::play::EntityEventPacket::kId, packet);
        return true;
    };
    operations["hurt_sound"] = [](const Environment&, ServerEntity& entity, const Arguments&) -> std::optional<Value> {
        return entity::entityCombat::hurtSound(entity);
    };
    operations["death_sound"] = [](const Environment&, ServerEntity& entity, const Arguments&) -> std::optional<Value> {
        return entity::entityCombat::deathSound(entity);
    };
    operations["pickup_animation"] = [](const Environment& environment, ServerEntity& entity,
                                        const Arguments& arguments) -> std::optional<Value> {
        const wire::protocol::play::TakeItemEntityPacket packet{
            .collectedEntityId = static_cast<std::int32_t>(asInt(at(arguments, 0))),
            .collectorEntityId = entity.entityId(),
            .pickupItemCount = static_cast<std::int32_t>(std::max<std::int64_t>(asInt(at(arguments, 1), 1), 1))};
        sendTo(environment, {}, wire::protocol::play::TakeItemEntityPacket::kId, packet);
        return true;
    };
    operations["tracked_by"] = [](const Environment& environment, ServerEntity& entity,
                                  const Arguments&) -> std::optional<Value> {
        std::vector<std::string> out;
        if (environment.world == nullptr || !environment.players) {
            return out;
        }
        for (player::PlayerSession* viewer : environment.players()) {
            if (viewer->worldName == environment.world->name() && viewer->connection().tracksEntity &&
                viewer->connection().tracksEntity(entity.entityId())) {
                out.push_back(viewer->uuid().toString());
            }
        }
        return out;
    };
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState

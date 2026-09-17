#include "TeleportMove.hpp"

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <memory>
#include <utility>

namespace basalt::server::runtime::command::commands::teleportMove {
namespace {

[[nodiscard]] world::LoadedWorld* worldNamed(const CommandContext& context, const std::string& name) {
    const auto& worlds = context.environment().options().worlds;
    const auto found = worlds.find(name);
    return found == worlds.end() ? nullptr : found->second;
}

/// parity: what `performTeleport` does after the move: no fall speed left, and standing.
void settle(entity::ServerEntity& entity, const Place& to) {
    entity.x = to.x;
    entity.y = to.y;
    entity.z = to.z;
    entity.yaw = to.yaw;
    entity.headYaw = to.yaw;
    entity.pitch = to.pitch;
    entity.velocityY = 0.0;
    entity.onGround = true;
    // parity: `PathNavigation.stop` for a pathfinding mob.
    entity.path.reset();
}

void movePlayer(player::PlayerSession& player, const Place& to) {
    // The world change sends the position itself, so a teleport after it would be a second move.
    const bool switched = to.world != nullptr && to.world->name() != player.worldName &&
                          player.connection().switchWorld &&
                          player.connection().switchWorld(*to.world, to.x, to.y, to.z);
    if (!switched && player.connection().teleportTo) {
        player.connection().teleportTo(to.x, to.y, to.z, to.yaw, to.pitch);
    }
}

void moveEntity(const CommandContext& context, entity::ServerEntity& entity, world::LoadedWorld* from,
                const Place& to) {
    if (to.world == nullptr || from == nullptr || to.world == from) {
        settle(entity, to);
        return;
    }
    const registry::EntityTypeEntry* type = registry::entityTypes::of(entity.type());
    if (type == nullptr) {
        return;
    }
    const std::int32_t id = context.environment().players().allocateEntityIds(entity::enderDragon::idSpan(type->id));
    std::unique_ptr<entity::ServerEntity> copy =
        entity::entityNbt::decode(entity::entityNbt::encode(entity), id, type->protocolId);
    if (copy == nullptr) {
        return;
    }
    settle(*copy, to);
    to.world->state().spawnEntity(std::move(copy));
    entity.removed = true;
}

void moveItem(const CommandTarget& target, world::LoadedWorld* from, const Place& to) {
    if (from == nullptr) {
        return;
    }
    std::optional<world::DroppedItem> item = from->state().removeItem(target.entityId());
    if (!item.has_value()) {
        return;
    }
    item->x = to.x;
    item->y = to.y;
    item->z = to.z;
    item->velocityY = 0.0;
    static_cast<void>((to.world == nullptr ? from : to.world)->state().spawnItem(std::move(*item)));
}

} // namespace

Parsed<CommandTarget> self(const CommandContext& context) {
    if (const ExecutionSource* execution = context.source().asExecution();
        execution != nullptr && execution->entity().has_value()) {
        return *execution->entity();
    }
    const Parsed<player::PlayerSession*> player = context.requireEntity();
    if (!player.has_value()) {
        return std::unexpected{player.error()};
    }
    return CommandTarget{**player};
}

void move(const CommandContext& context, const CommandTarget& target, const Place& to) {
    if (player::PlayerSession* player = target.player(); player != nullptr) {
        movePlayer(*player, to);
        return;
    }
    world::LoadedWorld* from = worldNamed(context, target.worldName());
    if (from == nullptr) {
        const Parsed<world::LoadedWorld*> running = context.world();
        from = running.has_value() ? *running : nullptr;
    }
    if (entity::ServerEntity* entity = target.entity(); entity != nullptr) {
        moveEntity(context, *entity, from, to);
        return;
    }
    if (target.type() == CommandTarget::itemType()) {
        moveItem(target, from, to);
    }
}

} // namespace basalt::server::runtime::command::commands::teleportMove

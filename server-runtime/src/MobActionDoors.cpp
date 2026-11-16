#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

namespace basalt::server::runtime {

using entity::ServerEntity;
using player::PlayerSession;

void MobActionSystem::blockProgress(ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stage) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->connection().sendBlockDestruction) {
            player->connection().sendBlockDestruction(mob.entityId(), x, y, z, stage, 0.0F);
        }
    }
}

void MobActionSystem::levelEvent(ServerEntity& mob, std::int32_t event, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t data) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const wire::protocol::play::LevelEventPacket packet{
        .event = event, .pos = wire::protocol::play::BlockPos{x, y, z}, .data = data};
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::LevelEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool MobActionSystem::breakDoor(ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return false;
    }
    const std::optional<std::int32_t> door = world->state().blockState(x, y, z);
    const std::int32_t air = server::registry::blocks::air().stateId;
    if (!door.has_value() || !world->events().allowed(&server::plugins::pluginEventTypes::entityBreakDoor,
                                                      [&](server::plugins::PluginEvent& event) {
                                                          plugins::hookEvents::entity(event, world->name(), mob);
                                                          plugins::hookEvents::position(event, world->name(), x, y, z);
                                                          event.set("block", plugins::hookEvents::blockName(*door));
                                                      })) {
        return false;
    }
    // parity: `Level.removeBlock`, with the neighbours told so the other half of the door goes too.
    if (world->state().setBlockState(x, y, z, air)) {
        const std::vector<PlayerSession*> watchers = watchersOf(*world);
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y, .z = z, .stateId = air});
        broadcaster_->notifyAround(*world, watchers, x, y, z, door);
    }
    return true;
}

bool MobActionSystem::hardDifficulty(const ServerEntity& mob) {
    const world::LoadedWorld* world = worldOf(mob);
    return world != nullptr && world->difficulty() == "hard";
}

} // namespace basalt::server::runtime

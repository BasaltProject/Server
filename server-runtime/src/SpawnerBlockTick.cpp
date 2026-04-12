#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobSpawnerSystem.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <array>
#include <optional>

namespace basalt::server::runtime {
namespace {

/// parity: `BaseSpawner.EVENT_SPAWN`, the only event the block answers.
constexpr std::int32_t kEventSpawn = 1;
/// parity: `ServerLevel.runBlockEvents`, which tells the players within sixty-four blocks.
constexpr double kBlockEventRange = 64.0;

constexpr std::string_view kSpawnerBlock = "spawner";

/// parity: `SpawnerBlockEntity.getUpdateTag`, the saved fields without the list of potentials.
[[nodiscard]] wire::nbt::NbtCompound updateTagOf(const wire::nbt::NbtCompound& data) {
    wire::nbt::NbtCompound tag = data;
    for (const std::string_view key : {"id", "x", "y", "z", "components", "SpawnPotentials"}) {
        static_cast<void>(tag.remove(key));
    }
    return tag;
}

} // namespace

bool RedstoneTickSystem::tickSpawner(world::LoadedWorld& world, wire::nbt::NbtCompound& data, std::int32_t x,
                                     std::int32_t y, std::int32_t z) {
    if (options_.mobSpawners == nullptr) {
        return false;
    }
    const std::array<std::int32_t, 3> pos{x, y, z};
    bool resend = false;
    const bool changed = options_.mobSpawners->tick(
        world, data, pos,
        // parity: `level.blockEvent(pos, Blocks.SPAWNER, id, 0)`, run with the other block events.
        [&world, &pos](std::int32_t eventId) {
            world.state().queueBlockEvent(world::BlockEvent{
                .x = pos[0], .y = pos[1], .z = pos[2], .block = std::string{kSpawnerBlock}, .first = eventId});
        },
        // parity: `SpawnerBlockEntity.setNextSpawnData`, whose `sendBlockUpdated` resends the block
        // entity; sent once the turn has written its data back.
        [&resend] { resend = true; });
    if (!resend) {
        return changed;
    }
    const std::optional<std::int32_t> typeId =
        server::registry::blockEntityTypes::idOf(wire::common::Identifier::vanilla("mob_spawner"));
    if (!typeId.has_value()) {
        return changed;
    }
    const wire::protocol::play::BlockEntityDataPacket packet{
        .pos = {.x = x, .y = y, .z = z}, .typeId = *typeId, .tag = updateTagOf(data)};
    for (player::PlayerSession* player : watchersOf(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    return changed;
}

bool RedstoneTickSystem::runSpawnerEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                                         const std::vector<player::PlayerSession*>& watchers) {
    if (event.block != kSpawnerBlock) {
        return false;
    }
    // parity: `doBlockEvent` drops an event whose square no longer holds a spawner, and
    // `SpawnerBlockEntity.triggerEvent` answers only its own event.
    const std::optional<std::int32_t> stateId = world.state().blockState(event.x, event.y, event.z);
    const wire::common::Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(event.x >> 4, event.z >> 4);
    if (block == nullptr || block->path() != kSpawnerBlock || chunk == nullptr ||
        chunk->blockEntityAt(event.x & 15, event.y, event.z & 15) == nullptr || event.first != kEventSpawn) {
        return true;
    }
    for (player::PlayerSession* player : watchers) {
        const double dx = event.x - player->x;
        const double dy = event.y - player->y;
        const double dz = event.z - player->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kBlockEventRange * kBlockEventRange &&
            player->connection().sendBlockEvent) {
            player->connection().sendBlockEvent(event.x, event.y, event.z, *stateId, event.first, event.second);
        }
    }
    return true;
}

} // namespace basalt::server::runtime

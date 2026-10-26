#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/world/DragonEggs.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

namespace basalt::server::runtime {

void BlockInteractionHandler::teleportDragonEgg(const wire::protocol::play::BlockPos& pos) {
    constexpr std::int32_t kTravelEvent = 2015;
    constexpr double kLevelEventRange = 64.0;
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || !world->state().gameplay().blocks.dragonEggTeleport) {
        return;
    }
    const server::world::ChunkHeight height = config::dimensionFacts(world->declaration().dimension).chunkHeight;
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::optional<world::dragonEggs::Moved> moved = world::dragonEggs::teleport(
        world->state(), pos.x, pos.y, pos.z, height.minY(), height.maxY() + 1, random,
        [world](std::int32_t x, std::int32_t z) { return world->border().contains(x + 0.5, z + 0.5); });
    if (!moved.has_value()) {
        return;
    }
    for (const player::PlayerSession* watcher : owner_->playersInSameWorld()) {
        const double dx = watcher->x - (pos.x + 0.5);
        const double dy = watcher->y - (pos.y + 0.5);
        const double dz = watcher->z - (pos.z + 0.5);
        if (watcher->connection().sendLevelEvent &&
            (dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange) {
            watcher->connection().sendLevelEvent(kTravelEvent, pos.x, pos.y, pos.z, moved->packedDifference);
        }
    }
    const wire::protocol::play::BlockPos target{.x = moved->toX, .y = moved->toY, .z = moved->toZ};
    const std::int32_t air = server::registry::blocks::air().stateId;
    broadcastBlockUpdate(target, moved->stateId);
    broadcastBlockUpdate(pos, air);
    notifyNeighbours(target);
    notifyNeighbours(pos);
}

} // namespace basalt::server::runtime

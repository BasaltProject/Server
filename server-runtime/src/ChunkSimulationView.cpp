#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace basalt::server::runtime {

ChunkSimulationView::ChunkSimulationView(player::PlayerRegistry& players) : players_(&players) {}

std::vector<std::array<std::int32_t, 2>> ChunkSimulationView::simulatedChunks(const world::LoadedWorld& world,
                                                                              const sched::Region* region,
                                                                              const std::int32_t extraRadius) const {
    const std::int32_t radius = std::min(world.settings().viewDistance, kVanillaSimulationDistance) + extraRadius;
    std::unordered_set<std::int64_t> seen;
    std::vector<std::array<std::int32_t, 2>> chunks;
    for (const player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        // Only this region's players: the chunks around a player always sit in the player's own region.
        if (region != nullptr && !region->ownsPosition(player->x, player->z)) {
            continue;
        }
        const auto centreX = static_cast<std::int32_t>(player->x) >> 4;
        const auto centreZ = static_cast<std::int32_t>(player->z) >> 4;
        for (std::int32_t x = centreX - radius; x <= centreX + radius; x++) {
            for (std::int32_t z = centreZ - radius; z <= centreZ + radius; z++) {
                if (region != nullptr && !region->owns(x, z)) {
                    continue;
                }
                const std::int64_t key = (static_cast<std::int64_t>(x) << 32) | static_cast<std::uint32_t>(z);
                if (seen.insert(key).second) {
                    chunks.push_back({x, z});
                }
            }
        }
    }
    // parity: `/forceload`.
    for (const std::int64_t key : world.forcedChunks()) {
        const std::int32_t x = world::ChunkGenerationQueue::unpackX(key);
        const std::int32_t z = world::ChunkGenerationQueue::unpackZ(key);
        if (region != nullptr && !region->owns(x, z)) {
            continue;
        }
        if (seen.insert(key).second) {
            chunks.push_back({x, z});
        }
    }
    return chunks;
}

} // namespace basalt::server::runtime

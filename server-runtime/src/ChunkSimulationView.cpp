#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <unordered_set>

namespace basalt::server::runtime {

ChunkSimulationView::ChunkSimulationView(player::PlayerRegistry& players) : players_(&players) {}

std::vector<std::array<std::int32_t, 2>> ChunkSimulationView::simulatedChunks(const world::LoadedWorld& world,
                                                                              const sched::Region* region) const {
    const std::int32_t radius = std::min(world.settings().viewDistance, kVanillaSimulationDistance);
    std::unordered_set<std::int64_t> seen;
    std::vector<std::array<std::int32_t, 2>> chunks;
    for (const player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name()) {
            continue;
        }
        // Only the players this region owns: the chunks around a player are always in the same
        // region as the player (a region is made of the sections that are loaded and touching, and a
        // player loads the ones around them), so walking every player's square in every region was
        // the same answer worked out a hundred times.
        if (region != nullptr && !region->ownsPosition(player->x, player->z)) {
            continue;
        }
        const auto centreX = static_cast<std::int32_t>(player->x) >> 4;
        const auto centreZ = static_cast<std::int32_t>(player->z) >> 4;
        for (std::int32_t x = centreX - radius; x <= centreX + radius; x++) {
            for (std::int32_t z = centreZ - radius; z <= centreZ + radius; z++) {
                // A chunk is simulated by whoever owns it, so two regions never tick the same one.
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
    return chunks;
}

} // namespace basalt::server::runtime

#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cstdint>
#include <bit>
#include <limits>
#include <vector>

namespace basalt::server::runtime {

namespace {
constexpr std::int64_t kEmptySlot = std::numeric_limits<std::int64_t>::min();

class KeySet {
public:
    explicit KeySet(std::size_t expected) {
        thread_local std::vector<std::int64_t> storage;
        table_ = &storage;
        std::size_t capacity = 64;
        while (capacity < expected * 2) {
            capacity <<= 1;
        }
        table_->assign(capacity, kEmptySlot);
        mask_ = capacity - 1;
    }

    bool insert(std::int64_t key) {
        std::size_t slot = (static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ULL >> 20) & mask_;
        while ((*table_)[slot] != kEmptySlot) {
            if ((*table_)[slot] == key) {
                return false;
            }
            slot = (slot + 1) & mask_;
        }
        (*table_)[slot] = key;
        return true;
    }

private:
    std::vector<std::int64_t>* table_;
    std::size_t mask_;
};
} // namespace

ChunkSimulationView::ChunkSimulationView(player::PlayerRegistry& players) : players_(&players) {}

std::vector<std::array<std::int32_t, 2>> ChunkSimulationView::simulatedChunks(const world::LoadedWorld& world,
                                                                              const sched::Region* region,
                                                                              const std::int32_t extraRadius) const {
    const std::int32_t radius = std::min(world.settings().viewDistance,
                                         world.tunables().simulationDistance.value_or(kVanillaSimulationDistance)) +
                                extraRadius;
    const auto snapshot = players_->snapshot();
    const auto side = static_cast<std::size_t>(2 * radius + 1);
    const std::size_t expected = snapshot.size() * side * side + world.forcedChunks().size();
    KeySet seen{expected};
    std::vector<std::array<std::int32_t, 2>> chunks;
    chunks.reserve(expected);
    for (const player::PlayerSession* player : snapshot) {
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
                if (seen.insert(key)) {
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
        if (seen.insert(key)) {
            chunks.push_back({x, z});
        }
    }
    return chunks;
}

} // namespace basalt::server::runtime

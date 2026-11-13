#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
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

std::shared_ptr<const ChunkSimulationView::Chunks>
ChunkSimulationView::simulatedChunks(const world::LoadedWorld& world, const sched::Region* region,
                                     const std::int32_t extraRadius) const {
    const std::int32_t radius = std::min(world.settings().viewDistance,
                                         world.tunables().simulationDistance.value_or(kVanillaSimulationDistance)) +
                                extraRadius;
    const auto snapshot = players_->snapshot();
    std::vector<std::array<std::int32_t, 2>> centres;
    for (const player::PlayerSession* player : snapshot) {
        if (player->worldName != world.name()) {
            continue;
        }
        // Only this region's players: the chunks around a player always sit in the player's own region.
        if (region != nullptr && !region->ownsPosition(player->x, player->z)) {
            continue;
        }
        centres.push_back({static_cast<std::int32_t>(player->x) >> 4, static_cast<std::int32_t>(player->z) >> 4});
    }
    const std::vector<std::int64_t> forced(world.forcedChunks().begin(), world.forcedChunks().end());
    const std::uint64_t generation = region != nullptr ? region->generation() : 0;

    {
        const std::lock_guard<std::mutex> guard{lock_};
        for (const Entry& entry : cache_) {
            if (entry.world == &world && entry.region == region && entry.extraRadius == extraRadius &&
                entry.generation == generation && entry.radius == radius && entry.centres == centres &&
                entry.forced == forced) {
                return entry.result;
            }
        }
    }

    const auto side = static_cast<std::size_t>(2 * radius + 1);
    const std::size_t expected = centres.size() * side * side + forced.size();
    KeySet seen{expected};
    auto chunks = std::make_shared<Chunks>();
    chunks->reserve(expected);
    for (const std::array<std::int32_t, 2>& centre : centres) {
        for (std::int32_t x = centre[0] - radius; x <= centre[0] + radius; x++) {
            for (std::int32_t z = centre[1] - radius; z <= centre[1] + radius; z++) {
                if (region != nullptr && !region->owns(x, z)) {
                    continue;
                }
                const std::int64_t key = (static_cast<std::int64_t>(x) << 32) | static_cast<std::uint32_t>(z);
                if (seen.insert(key)) {
                    chunks->push_back({x, z});
                }
            }
        }
    }
    // parity: `/forceload`.
    for (const std::int64_t key : forced) {
        const std::int32_t x = world::ChunkGenerationQueue::unpackX(key);
        const std::int32_t z = world::ChunkGenerationQueue::unpackZ(key);
        if (region != nullptr && !region->owns(x, z)) {
            continue;
        }
        if (seen.insert(key)) {
            chunks->push_back({x, z});
        }
    }

    std::shared_ptr<const Chunks> result = std::move(chunks);
    const std::lock_guard<std::mutex> guard{lock_};
    constexpr std::size_t kMaxEntries = 64;
    for (Entry& entry : cache_) {
        if (entry.world == &world && entry.region == region && entry.extraRadius == extraRadius) {
            entry = Entry{&world, region, extraRadius, generation, radius, std::move(centres), forced, result, nullptr};
            return result;
        }
    }
    if (cache_.size() >= kMaxEntries) {
        cache_.clear();
    }
    cache_.push_back(
        Entry{&world, region, extraRadius, generation, radius, std::move(centres), forced, result, nullptr});
    return result;
}

std::shared_ptr<const ChunkSimulationView::Keys>
ChunkSimulationView::simulatedKeys(const world::LoadedWorld& world, const sched::Region* region,
                                   const std::int32_t extraRadius) const {
    const std::shared_ptr<const Chunks> chunks = simulatedChunks(world, region, extraRadius);
    {
        const std::lock_guard<std::mutex> guard{lock_};
        for (const Entry& entry : cache_) {
            if (entry.result == chunks && entry.keys != nullptr) {
                return entry.keys;
            }
        }
    }
    auto built = std::make_shared<Keys>();
    built->reserve(chunks->size());
    for (const std::array<std::int32_t, 2>& at : *chunks) {
        built->insert((static_cast<std::int64_t>(at[0]) << 32) | static_cast<std::uint32_t>(at[1]));
    }
    const std::lock_guard<std::mutex> guard{lock_};
    for (Entry& entry : cache_) {
        if (entry.result == chunks) {
            entry.keys = built;
            break;
        }
    }
    return built;
}

} // namespace basalt::server::runtime

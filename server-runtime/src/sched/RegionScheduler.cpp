#include "RegionWorkers.hpp"

#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::sched {
namespace {

/// The chunk a world coordinate is in.
///
/// parity: `SectionPos.blockToSectionCoord(Mth.floor(x))`, which is a floor and then a shift. The
/// frozen Kotlin subtracted almost sixteen from a negative coordinate instead, and that answers one
/// chunk too far west at exact negative multiples of sixteen: x of -16.0 is the first block of
/// chunk -1, not the last of chunk -2.
[[nodiscard]] std::int32_t floorToChunk(double value) { return wire::common::mth::floor(value) >> 4; }

static_assert(regionMap::sectionOf(-1) == -1, "a shift, not a division: chunk -1 is in section -1");

} // namespace

bool Region::owns(std::int32_t chunkX, std::int32_t chunkZ) const {
    return sections_.empty() || sections_.contains(regionMap::sectionKeyOfChunk(chunkX, chunkZ));
}

bool Region::ownsPosition(double x, double z) const { return owns(floorToChunk(x), floorToChunk(z)); }

RegionScheduler::RegionScheduler(bool regional, std::int32_t threads)
    : regional_(regional), workers_(regional && threads > 1 ? std::make_unique<RegionWorkers>(threads) : nullptr) {}

RegionScheduler::~RegionScheduler() = default;

std::vector<Region*> RegionScheduler::regions() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::vector<Region*> every;
    for (const WorldRegions& world : byWorld_) {
        for (const std::unique_ptr<Region>& region : world.regions) {
            every.push_back(region.get());
        }
    }
    return every;
}

std::vector<Region*> RegionScheduler::regionsOf(std::string_view worldName) const {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::vector<Region*> found;
    const auto at =
        std::ranges::find_if(byWorld_, [worldName](const WorldRegions& world) { return world.name == worldName; });
    if (at == byWorld_.end()) {
        return found;
    }
    found.reserve(at->regions.size());
    for (const std::unique_ptr<Region>& region : at->regions) {
        found.push_back(region.get());
    }
    return found;
}

Region& RegionScheduler::regionFor(std::string_view worldName, std::int32_t chunkX, std::int32_t chunkZ) {
    const std::lock_guard<std::mutex> lock{mutex_};
    WorldRegions& world = worldLocked(worldName);
    if (!world.regions.empty()) {
        for (const std::unique_ptr<Region>& region : world.regions) {
            if (region->owns(chunkX, chunkZ)) {
                return *region;
            }
        }
        return *world.regions.front();
    }
    return newRegionLocked(worldName);
}

RegionScheduler::WorldRegions& RegionScheduler::worldLocked(std::string_view worldName) {
    const auto at =
        std::ranges::find_if(byWorld_, [worldName](const WorldRegions& world) { return world.name == worldName; });
    if (at != byWorld_.end()) {
        return *at;
    }
    byWorld_.push_back(WorldRegions{.name = std::string{worldName}, .regions = {}});
    return byWorld_.back();
}

Region& RegionScheduler::newRegionLocked(std::string_view worldName) {
    WorldRegions& world = worldLocked(worldName);
    world.regions.push_back(std::unique_ptr<Region>{new Region{nextId_++, std::string{worldName}}});
    return *world.regions.back();
}

void RegionScheduler::repartition(std::string_view worldName, std::span<const std::int64_t> loadedChunks,
                                  const ChangeHandler& onChange) {
    if (!regional_) {
        return;
    }
    std::unordered_set<std::int64_t> sections;
    sections.reserve(loadedChunks.size());
    for (const std::int64_t chunk : loadedChunks) {
        sections.insert(
            regionMap::sectionKeyOfChunk(static_cast<std::int32_t>(chunk >> 32), static_cast<std::int32_t>(chunk)));
    }
    const std::vector<std::unordered_set<std::int64_t>> groups = regionMap::components(sections);
    if (groups.empty()) {
        return;
    }

    const std::lock_guard<std::mutex> lock{mutex_};
    std::vector<std::unique_ptr<Region>>& current = worldLocked(worldName).regions;
    if (current.empty()) {
        static_cast<void>(newRegionLocked(worldName));
    }
    std::int32_t merged = 0;
    std::int32_t split = 0;
    std::vector<Region*> kept;
    std::vector<Region*> spare;
    spare.reserve(current.size());
    for (const std::unique_ptr<Region>& region : current) {
        spare.push_back(region.get());
    }

    for (const std::unordered_set<std::int64_t>& group : groups) {
        const auto overlap = [&group](const Region* region) {
            return std::ranges::count_if(region->sections(),
                                         [&group](std::int64_t section) { return group.contains(section); });
        };
        std::vector<Region*> owners;
        for (Region* region : spare) {
            if (overlap(region) > 0) {
                owners.push_back(region);
            }
        }
        Region* survivor = nullptr;
        if (!owners.empty()) {
            // Whoever already owns most of this group keeps it, so the fewest tasks move.
            survivor = *std::ranges::max_element(
                owners, [&overlap](const Region* left, const Region* right) { return overlap(left) < overlap(right); });
        } else {
            // A group nobody owned yet: the world's first free region takes it if there is one, and
            // otherwise it is a new one.
            const auto free =
                std::ranges::find_if(spare, [](const Region* region) { return region->sections().empty(); });
            if (free != spare.end()) {
                survivor = *free;
            } else {
                survivor = &newRegionLocked(worldName);
                ++split;
            }
        }
        for (Region* other : owners) {
            if (other == survivor) {
                continue;
            }
            // Everything it had queued belongs to the survivor now, not to nobody.
            other->owner().drainInto(survivor->owner());
            // Closed although it is about to be dropped: nothing can be handed to it between here
            // and the end of this repartition, and a caller that kept a pointer is told why.
            other->owner().close();
            std::erase(spare, other);
            ++merged;
        }
        survivor->sections_ = group;
        std::erase(spare, survivor);
        kept.push_back(survivor);
    }

    // What is left owned sections nobody has loaded any more: its work goes to the first region of
    // the world rather than being dropped on the floor.
    for (Region* gone : spare) {
        gone->owner().drainInto(kept.front()->owner());
        gone->owner().close();
    }

    std::vector<std::unique_ptr<Region>> rebuilt;
    rebuilt.reserve(kept.size());
    for (Region* region : kept) {
        const auto at =
            std::ranges::find_if(current, [region](const std::unique_ptr<Region>& one) { return one.get() == region; });
        rebuilt.push_back(std::move(*at));
    }
    current = std::move(rebuilt);
    for (std::size_t index = 0; index < current.size(); ++index) {
        current[index]->isPrimary_ = index == 0;
    }
    if ((merged > 0 || split > 0) && onChange) {
        onChange(merged, split);
    }
}

void RegionScheduler::tick(const FailureHandler& onFailure, const std::function<void(Region&)>& tickRegion) {
    if (!global_.isCurrent()) {
        global_.bindToCurrentThread();
    }
    global_.advanceTick();
    global_.runPending(onFailure);
    const std::vector<Region*> turn = regions();
    if (workers_ == nullptr || turn.size() < 2) {
        for (Region* region : turn) {
            if (!region->owner().isCurrent()) {
                region->owner().bindToCurrentThread();
            }
            tickRegion(*region);
        }
    } else {
        workers_->runTurn(turn, onFailure, tickRegion);
    }
    global_.runEndOfTick(onFailure);
}

void RegionScheduler::close() {
    if (workers_ != nullptr) {
        workers_->stop();
    }
    global_.close();
    for (Region* region : regions()) {
        region->owner().close();
    }
}

} // namespace basalt::server::runtime::sched

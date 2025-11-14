#pragma once

#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/server/runtime/sched/RegionMap.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::sched {

class RegionScheduler;
class RegionWorkers;

/// A set of chunks that tick together, and the owner that ticks them.
///
/// With the world in one piece there is one region per world and it owns every chunk of it. When a
/// world is split there are several, each with its own chunks. Nothing above this changes when that
/// happens: the callers already ask `RegionScheduler::regionFor` rather than assuming.
class Region {
public:
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
    Region(Region&&) = delete;
    Region& operator=(Region&&) = delete;
    ~Region() = default;

    /// Unique for the life of the process, so a log line can name which one did something.
    [[nodiscard]] std::int32_t id() const noexcept { return id_; }

    /// The world it belongs to. A region never spans two.
    [[nodiscard]] const std::string& worldName() const noexcept { return worldName_; }

    [[nodiscard]] Owner& owner() noexcept { return owner_; }

    [[nodiscard]] const Owner& owner() const noexcept { return owner_; }

    /// Whether this region is the one that runs what is not split yet.
    ///
    /// A world tick still has parts that are the whole world's rather than a region's: the weather,
    /// the clock, the sleep vote. Running them once per region would run them several times, so
    /// they run in the first region of the world and are named here rather than left to luck.
    [[nodiscard]] bool isPrimary() const noexcept { return isPrimary_; }

    /// The grid sections this region covers, empty while the world is in one piece.
    ///
    /// Empty means "everything": a world that is not split has one region and it owns whatever is
    /// loaded, without a set to keep up to date.
    [[nodiscard]] const std::unordered_set<std::int64_t>& sections() const noexcept { return sections_; }

    /// Whether this region owns that chunk. True for everything while the world is one region.
    [[nodiscard]] bool owns(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Whether this region owns the block, entity or player at these world coordinates.
    [[nodiscard]] bool ownsPosition(double x, double z) const;

private:
    friend class RegionScheduler;

    Region(std::int32_t id, std::string worldName) : id_(id), worldName_(std::move(worldName)), owner_(worldName_) {}

    std::int32_t id_;
    std::string worldName_;
    Owner owner_;
    std::unordered_set<std::int64_t> sections_;
    bool isPrimary_ = true;
};

/// Which regions exist and who ticks them.
///
/// parity of model with `TickRegions`: how many regions there are is not a setting, it is a
/// consequence of where the players are; what the operator sets is whether the world is split at
/// all and how many threads tick regions at once. With `regional` off every world is one region,
/// which is the vanilla order of tick.
///
/// There is also one owner that belongs to nobody's region: the hour, the weather, the world border
/// and everything else that is global by nature. It is not "region zero" with a special case, it is
/// its own owner, and it exists in both modes.
class RegionScheduler {
public:
    /// What a repartition changed, for whoever wants to log it.
    using ChangeHandler = std::function<void(std::int32_t merged, std::int32_t split)>;

    /// `regional` is whether a world may be split into several regions, off being the vanilla order
    /// of tick; `threads` is how many regions tick at once when it is on, one meaning they take
    /// turns.
    explicit RegionScheduler(bool regional = false, std::int32_t threads = 1);

    RegionScheduler(const RegionScheduler&) = delete;
    RegionScheduler& operator=(const RegionScheduler&) = delete;
    RegionScheduler(RegionScheduler&&) = delete;
    RegionScheduler& operator=(RegionScheduler&&) = delete;
    ~RegionScheduler();

    [[nodiscard]] bool regional() const noexcept { return regional_; }

    /// parity: Folia's own global region, for what does not belong to a piece of the map.
    [[nodiscard]] Owner& global() noexcept { return global_; }

    /// Every region there is, world by world, in the order they were made.
    [[nodiscard]] std::vector<Region*> regions() const;

    /// The regions of one world. One today; the ones the world is split into when that lands.
    [[nodiscard]] std::vector<Region*> regionsOf(std::string_view worldName) const;

    /// Who owns this position, making the world's first region if it has none yet.
    ///
    /// The coordinates decide once a world is split; while it is one region they are ignored, and
    /// that is the point of asking with them anyway: the call sites are already right for the day
    /// they stop being ignored. The answer must not be kept from one turn to the next, because a
    /// region can merge into another between two of them.
    Region& regionFor(std::string_view worldName, std::int32_t chunkX = 0, std::int32_t chunkZ = 0);

    /// Works out what the regions of a world should be from the chunks it has loaded, and makes it
    /// so: groups that grew together are merged, a group that fell apart is split.
    ///
    /// Only ever called at a tick boundary, before any region has started its turn: an entity or a
    /// block changing hands mid-tick is the one thing this model cannot survive. With `regional`
    /// off it does nothing at all, which is what keeps the default mode on exactly one region.
    ///
    /// A merge moves the work queued on the regions that go away onto the survivor. A split cannot:
    /// a task does not say which half it belonged to, so the survivor keeps it, which is what
    /// Folia's own comment about "the region that keeps the data" amounts to.
    void repartition(std::string_view worldName, std::span<const std::int64_t> loadedChunks,
                     const ChangeHandler& onChange = {});

    /// One turn of everything: the global owner first, then every region.
    ///
    /// `tickRegion` is what a region does with its own turn, which is the world tick. The order
    /// here is the order a single thread runs them in, and it is what makes the default mode the
    /// same game as before: with `regional` off there is one region per world and they run in the
    /// order the worlds were loaded, exactly as the loop did when it walked the worlds itself.
    void tick(const FailureHandler& onFailure, const std::function<void(Region&)>& tickRegion);

    /// Nothing else runs anywhere: the server is going down.
    void close();

private:
    /// One world and the regions it is currently cut into.
    struct WorldRegions {
        std::string name;
        std::vector<std::unique_ptr<Region>> regions;
    };

    Region& newRegionLocked(std::string_view worldName);
    WorldRegions& worldLocked(std::string_view worldName);

    bool regional_;
    mutable std::mutex mutex_;

    /// Worlds in the order they were loaded, which is the order they tick in. A map would sort them
    /// by name and quietly change which world ticks first, and the order of tick is observable.
    std::vector<WorldRegions> byWorld_;
    std::int32_t nextId_ = 0;
    Owner global_{"global"};

    /// The threads that tick regions at once, or null when they take turns on the caller's thread.
    ///
    /// One thread is not the same as none: with one worker the regions still run somewhere else,
    /// which is measurable and pointless, so a single thread means the caller does it.
    std::unique_ptr<RegionWorkers> workers_;
};

} // namespace basalt::server::runtime::sched

#pragma once

#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/server/runtime/sched/RegionMap.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
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

class Region {
public:
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
    Region(Region&&) = delete;
    Region& operator=(Region&&) = delete;
    ~Region() = default;

    [[nodiscard]] std::int32_t id() const noexcept { return id_; }

    /// The world it belongs to. A region never spans two.
    [[nodiscard]] const std::string& worldName() const noexcept { return worldName_; }

    [[nodiscard]] Owner& owner() noexcept { return *owner_; }

    [[nodiscard]] const Owner& owner() const noexcept { return *owner_; }

    /// Outlives the region, so a session that kept it after a merge finds a closed owner instead of freed memory.
    [[nodiscard]] std::shared_ptr<Owner> sharedOwner() const noexcept { return owner_; }

    [[nodiscard]] bool isPrimary() const noexcept { return isPrimary_; }

    [[nodiscard]] const std::unordered_set<std::int64_t>& sections() const noexcept { return sections_; }

    /// Unique across all regions ever made; changes whenever the owned sections change.
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_.load(std::memory_order_acquire); }

    [[nodiscard]] bool owns(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] bool ownsPosition(double x, double z) const;

private:
    friend class RegionScheduler;

    Region(std::int32_t id, std::string worldName)
        : id_(id), worldName_(std::move(worldName)), owner_(std::make_shared<Owner>(worldName_)) {}

    std::int32_t id_;
    std::string worldName_;
    std::shared_ptr<Owner> owner_;
    std::unordered_set<std::int64_t> sections_;
    std::atomic<std::uint64_t> generation_{nextGeneration()};
    static std::uint64_t nextGeneration() noexcept;
    bool isPrimary_ = true;
};

class RegionScheduler {
public:
    using ChangeHandler = std::function<void(std::int32_t merged, std::int32_t split)>;

    explicit RegionScheduler(bool regional = false, std::int32_t threads = 1);

    RegionScheduler(const RegionScheduler&) = delete;
    RegionScheduler& operator=(const RegionScheduler&) = delete;
    RegionScheduler(RegionScheduler&&) = delete;
    RegionScheduler& operator=(RegionScheduler&&) = delete;
    ~RegionScheduler();

    [[nodiscard]] bool regional() const noexcept { return regional_; }

    /// parity: Folia's own global region, for what does not belong to a piece of the map.
    [[nodiscard]] Owner& global() noexcept { return global_; }

    [[nodiscard]] std::vector<Region*> regions() const;

    [[nodiscard]] std::vector<Region*> regionsOf(std::string_view worldName) const;

    /// The world's first region owner, creating the region on first use.
    [[nodiscard]] Owner* primaryOwner(std::string_view worldName);

    Region& regionFor(std::string_view worldName, std::int32_t chunkX = 0, std::int32_t chunkZ = 0);

    void repartition(std::string_view worldName, std::span<const std::int64_t> loadedChunks,
                     const ChangeHandler& onChange = {});

    void tick(const FailureHandler& onFailure, const std::function<void(Region&)>& tickRegion);

    void close();

private:
    struct WorldRegions {
        std::string name;
        std::vector<std::unique_ptr<Region>> regions;
    };

    Region& newRegionLocked(std::string_view worldName);
    WorldRegions& worldLocked(std::string_view worldName);

    bool regional_;
    mutable std::mutex mutex_;

    std::vector<WorldRegions> byWorld_;
    std::deque<std::unique_ptr<Region>> graveyard_;
    std::int32_t nextId_ = 0;
    Owner global_{"global"};

    std::unique_ptr<RegionWorkers> workers_;
};

} // namespace basalt::server::runtime::sched

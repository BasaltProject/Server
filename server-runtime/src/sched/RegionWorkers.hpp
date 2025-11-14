#pragma once

#include <basalt/server/runtime/sched/Owner.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace basalt::server::runtime::sched {

class Region;

/// A fixed set of threads that tick regions at the same time, with a barrier at the end.
///
/// Every region has to finish before the next tick starts: a region that ran ahead would see a
/// world a tick older than its neighbour's, and what crosses a border between them would be read
/// twice or not at all. parity of intent with `TickRegionScheduler`, minus its earliest-deadline
/// ordering, which is worth having only once there are more regions than threads to measure it
/// with.
class RegionWorkers {
public:
    /// A turn that takes this long has already gone wrong. The barrier is a safety net, not a
    /// deadline: waiting a little longer is better than carrying on with half a tick.
    static constexpr std::chrono::seconds kTurnLimit{30};

    explicit RegionWorkers(std::int32_t threads);

    RegionWorkers(const RegionWorkers&) = delete;
    RegionWorkers& operator=(const RegionWorkers&) = delete;
    RegionWorkers(RegionWorkers&&) = delete;
    RegionWorkers& operator=(RegionWorkers&&) = delete;
    ~RegionWorkers();

    void runTurn(const std::vector<Region*>& regions, const FailureHandler& onFailure,
                 const std::function<void(Region&)>& work);

    void stop();

private:
    void workerLoop();

    mutable std::mutex mutex_;
    /// Woken when a turn starts or the pool stops, which is what replaces polling for work.
    std::condition_variable wake_;
    /// Woken as each region finishes, which is what the barrier waits on.
    std::condition_variable done_;

    bool running_ = true;
    std::vector<Region*> turn_;
    std::size_t claimed_ = 0;
    std::size_t finished_ = 0;
    const std::function<void(Region&)>* work_ = nullptr;
    const FailureHandler* onFailure_ = nullptr;

    std::vector<std::thread> threads_;
};

} // namespace basalt::server::runtime::sched

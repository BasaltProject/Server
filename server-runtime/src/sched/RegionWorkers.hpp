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

/// A fixed set of threads ticking regions at once, with a barrier: a region running ahead would see an older neighbour.
class RegionWorkers {
public:
    /// A turn this long has already gone wrong; the barrier is a safety net, not a deadline.
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
    std::condition_variable wake_;
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

#include "RegionWorkers.hpp"

#include <basalt/server/runtime/sched/RegionScheduler.hpp>

#include <stdexcept>
#include <string>

namespace basalt::server::runtime::sched {

RegionWorkers::RegionWorkers(std::int32_t threads) {
    threads_.reserve(static_cast<std::size_t>(threads));
    for (std::int32_t index = 0; index < threads; ++index) {
        threads_.emplace_back([this] { workerLoop(); });
    }
}

RegionWorkers::~RegionWorkers() {
    stop();
    for (std::thread& worker : threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void RegionWorkers::workerLoop() {
    while (true) {
        Region* region = nullptr;
        const std::function<void(Region&)>* work = nullptr;
        const FailureHandler* onFailure = nullptr;
        {
            // Everything about a turn is read under the same lock that writes it: a worker that
            // read the list without it kept a turn that was already over and ticked a region a
            // second time, which looked like a corrupted world rather than like a scheduling bug.
            std::unique_lock<std::mutex> lock{mutex_};
            wake_.wait(lock, [this] { return !running_ || claimed_ < turn_.size(); });
            if (!running_) {
                return;
            }
            region = turn_[claimed_++];
            work = work_;
            onFailure = onFailure_;
        }
        // Claiming it again from the same worker would be the same store, and this is once a tick
        // per region: worth the load to skip it.
        if (!region->owner().isCurrent()) {
            region->owner().bindToCurrentThread();
        }
        try {
            (*work)(*region);
        } catch (const std::exception& failure) {
            (*onFailure)(failure);
        }
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            ++finished_;
        }
        done_.notify_all();
    }
}

void RegionWorkers::runTurn(const std::vector<Region*>& regions, const FailureHandler& onFailure,
                            const std::function<void(Region&)>& work) {
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        turn_ = regions;
        claimed_ = 0;
        finished_ = 0;
        work_ = &work;
        onFailure_ = &onFailure;
    }
    wake_.notify_all();

    // The barrier: nobody starts the next tick until every region has finished this one.
    bool timedOut = false;
    std::size_t finished = 0;
    {
        std::unique_lock<std::mutex> lock{mutex_};
        timedOut = !done_.wait_for(lock, kTurnLimit, [this, &regions] { return finished_ >= regions.size(); });
        finished = finished_;
        turn_.clear();
        claimed_ = 0;
        work_ = nullptr;
        onFailure_ = nullptr;
    }
    if (timedOut) {
        // Never expected: a region that does not finish holds up every other one, so it is worth a
        // line saying how many did rather than a tick that is silently thirty seconds long.
        onFailure(std::runtime_error{"A turn timed out: " + std::to_string(finished) + " of " +
                                     std::to_string(regions.size()) + " region(s) finished"});
    }
}

void RegionWorkers::stop() {
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        running_ = false;
    }
    wake_.notify_all();
}

} // namespace basalt::server::runtime::sched

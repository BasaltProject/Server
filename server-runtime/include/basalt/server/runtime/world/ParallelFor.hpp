#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace basalt::server::runtime::world {

/// Runs `work(i)` for every i below `count`, over all cores when `parallel`; returns once all are done.
template <typename Work>
void parallelFor(std::size_t count, bool parallel, Work work) {
    const std::size_t threads =
        parallel ? std::min<std::size_t>(count, std::max(1U, std::thread::hardware_concurrency())) : 1;
    if (threads <= 1) {
        for (std::size_t at = 0; at < count; ++at) {
            work(at);
        }
        return;
    }
    std::atomic<std::size_t> next{0};
    const auto drain = [&next, count, &work] {
        for (std::size_t at = next.fetch_add(1); at < count; at = next.fetch_add(1)) {
            work(at);
        }
    };
    std::vector<std::jthread> helpers;
    helpers.reserve(threads - 1);
    for (std::size_t helper = 1; helper < threads; ++helper) {
        helpers.emplace_back(drain);
    }
    drain();
}

} // namespace basalt::server::runtime::world

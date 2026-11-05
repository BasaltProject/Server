#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace basalt::server::runtime {

/// Hands memory the allocator is holding on to back to the system. glibc keeps freed pages in per-thread arenas, so
/// after chunks unload the process stays as large as it was at its peak. A no-op where the allocator already returns
/// it.
class MemoryReleaser {
public:
    MemoryReleaser();
    ~MemoryReleaser();
    MemoryReleaser(const MemoryReleaser&) = delete;
    MemoryReleaser& operator=(const MemoryReleaser&) = delete;

    /// Caps the arenas and fixes the thresholds, once, before the worker threads start allocating.
    static void configureAllocator() noexcept;

    /// Asks for a release soon, off the calling thread.
    void request();

private:
    std::mutex mutex_;
    std::condition_variable_any wake_;
    bool pending_ = false;
    std::jthread worker_;
};

} // namespace basalt::server::runtime

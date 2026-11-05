#include <basalt/server/runtime/MemoryReleaser.hpp>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace basalt::server::runtime {

void MemoryReleaser::configureAllocator() noexcept {
#if defined(__GLIBC__)
    // Few arenas: a dozen threads would each keep their own freed pages. A fixed mmap threshold stops big buffers
    // (regions, chunk snapshots) from growing the heap and fragmenting it, and returns them on free.
    mallopt(M_ARENA_MAX, 4);
    mallopt(M_MMAP_THRESHOLD, 256 * 1024);
    mallopt(M_TRIM_THRESHOLD, 4 * 1024 * 1024);
#endif
}

MemoryReleaser::MemoryReleaser() {
#if defined(__GLIBC__)
    worker_ = std::jthread{[this](const std::stop_token& stop) {
        std::unique_lock lock{mutex_};
        while (!stop.stop_requested()) {
            if (!wake_.wait(lock, stop, [this] { return pending_; })) {
                return;
            }
            pending_ = false;
            lock.unlock();
            malloc_trim(0);
            lock.lock();
        }
    }};
#endif
}

MemoryReleaser::~MemoryReleaser() {
    if (worker_.joinable()) {
        worker_.request_stop();
        wake_.notify_all();
    }
}

void MemoryReleaser::request() {
    if (!worker_.joinable()) {
        return;
    }
    {
        const std::scoped_lock lock{mutex_};
        pending_ = true;
    }
    wake_.notify_one();
}

} // namespace basalt::server::runtime

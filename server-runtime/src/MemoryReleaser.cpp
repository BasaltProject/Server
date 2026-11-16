#include <basalt/server/runtime/MemoryReleaser.hpp>

#if defined(__GLIBC__)
#include <malloc.h>
#endif
#if defined(__linux__)
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/mman.h>
#endif

namespace basalt::server::runtime {

namespace {

#if defined(__linux__)
// Clean read-only file mappings (rodata, jars) only hold page cache copies: dropping them from this process is free.
void dropCleanFileMappings() {
    std::ifstream smaps{"/proc/self/smaps"};
    std::string line;
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    bool candidate = false;
    const auto flush = [&] {
        if (candidate && end > begin) {
            madvise(reinterpret_cast<void*>(begin), end - begin, MADV_DONTNEED);
        }
        candidate = false;
    };
    while (std::getline(smaps, line)) {
        const std::string_view view{line};
        const std::size_t dash = view.find('-');
        if (!view.empty() && (std::isxdigit(static_cast<unsigned char>(view[0])) != 0) &&
            dash != std::string_view::npos) {
            flush();
            std::from_chars(view.data(), view.data() + dash, begin, 16);
            const std::size_t space = view.find(' ');
            std::from_chars(view.data() + dash + 1, view.data() + space, end, 16);
            const std::string_view perms = view.substr(space + 1, 4);
            const std::size_t slash = view.find('/');
            candidate = perms.size() == 4 && perms[0] == 'r' && perms[1] == '-' && perms[2] == '-' &&
                        slash != std::string_view::npos && !view.substr(slash).starts_with("/dev/") &&
                        !view.substr(slash).starts_with("/memfd:");
        } else if (candidate && (view.starts_with("Anonymous:") || view.starts_with("Private_Dirty:") ||
                                 view.starts_with("Shared_Dirty:"))) {
            const std::size_t digits = view.find_first_of("0123456789");
            if (digits != std::string_view::npos && view[digits] != '0') {
                candidate = false;
            }
        }
    }
    flush();
}
#endif

} // namespace

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
            const bool idle = idle_;
            lock.unlock();
            malloc_trim(0);
#if defined(__linux__)
            if (idle) {
                dropCleanFileMappings();
            }
#endif
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

void MemoryReleaser::request(const bool idle) {
    if (!worker_.joinable()) {
        return;
    }
    {
        const std::scoped_lock lock{mutex_};
        pending_ = true;
        idle_ = idle;
    }
    wake_.notify_one();
}

} // namespace basalt::server::runtime

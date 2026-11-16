#pragma once

#include <basalt/metamorph/region/BswRegion.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::world {

/// Chunk storage over `.bsw` region files: one compressed blob per chunk, so a read unpacks one chunk, not a region.
class BswChunkStore {
public:
    static constexpr std::size_t kMaxOpenRegions = 64;
    static constexpr std::int64_t kCacheBytes = 4LL * 1024 * 1024;
    static constexpr int kLevel = 3;
    static constexpr std::size_t kSampleChunks = 192;
    static constexpr std::size_t kDictionaryBytes = 112640;

    enum class Read : std::uint8_t { Ok, Absent, Corrupt, Unreadable };

    explicit BswChunkStore(std::string regionFolder);
    ~BswChunkStore();

    BswChunkStore(const BswChunkStore&) = delete;
    BswChunkStore& operator=(const BswChunkStore&) = delete;

    /// Staged chunks that are not flushed yet answer first.
    [[nodiscard]] Read read(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t>& out);

    [[nodiscard]] bool has(std::int32_t chunkX, std::int32_t chunkZ);

    /// Empty `nbt` removes the chunk. Nothing reaches the disk until `flush`.
    void stage(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> nbt, std::int32_t timestamp);

    /// One commit per touched region; answers how many regions were written.
    std::size_t flush(bool parallel);

    [[nodiscard]] std::vector<std::int32_t> stored(std::int32_t regionX, std::int32_t regionZ);

    [[nodiscard]] std::int64_t residentBytes();

    [[nodiscard]] std::size_t openRegions();

    [[nodiscard]] bool hasDictionary();

    /// Joins the background worker; nothing is compacted or trained afterwards.
    void stop();

    [[nodiscard]] static std::string pathOf(const std::string& folder, std::int32_t regionX, std::int32_t regionZ);

private:
    struct Pending {
        std::vector<std::uint8_t> nbt;
        std::int32_t timestamp = 0;
    };

    using PendingRegion = std::unordered_map<std::uint32_t, Pending>;

    struct Slot {
        std::int64_t key = 0;
        std::shared_ptr<metamorph::region::BswRegion> region;
    };

    struct Cached {
        std::int64_t key = 0;
        std::vector<std::uint8_t> nbt;
    };

    [[nodiscard]] static std::int64_t regionKey(std::int32_t regionX, std::int32_t regionZ);
    [[nodiscard]] static std::int64_t chunkKey(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::shared_ptr<metamorph::region::BswRegion> acquire(std::int32_t regionX, std::int32_t regionZ,
                                                                        bool create, bool& unreadable);
    [[nodiscard]] const Pending* pendingLocked(std::int32_t chunkX, std::int32_t chunkZ) const;
    void cacheLocked(std::int64_t key, const std::vector<std::uint8_t>& nbt);
    void touchActivity();
    void work(std::stop_token stop);
    void trainDictionary(std::vector<std::vector<std::uint8_t>> samples);
    void compactIdle(std::stop_token stop);

    std::string folder_;
    wire::common::Logger logger_{"Regions"};

    std::mutex mutex_;
    std::list<Slot> open_;
    std::unordered_map<std::int64_t, std::list<Slot>::iterator> openByKey_;
    std::unordered_set<std::int64_t> broken_;
    std::unordered_map<std::int64_t, PendingRegion> pending_;
    std::unordered_map<std::int64_t, PendingRegion> flushing_;
    std::int64_t pendingBytes_ = 0;
    std::list<Cached> cache_;
    std::unordered_map<std::int64_t, std::list<Cached>::iterator> cacheByKey_;
    std::int64_t cacheBytes_ = 0;
    std::shared_ptr<const metamorph::region::BswDictionary> dictionary_;
    std::vector<std::vector<std::uint8_t>> samples_;
    bool trainRequested_ = false;
    bool trainSettled_ = false;
    std::set<std::int64_t> compactQueue_;

    std::mutex flushMutex_;
    std::atomic<std::int64_t> lastActivityMillis_{0};
    std::condition_variable_any wake_;
    std::jthread worker_;
};

} // namespace basalt::server::runtime::world

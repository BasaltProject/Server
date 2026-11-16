#include <basalt/metamorph/region/BswConvert.hpp>
#include <basalt/server/runtime/world/BswChunkStore.hpp>
#include <basalt/server/runtime/world/ParallelFor.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using metamorph::region::BswCodec;
using metamorph::region::BswDictionary;
using metamorph::region::BswOptions;
using metamorph::region::BswRead;
using metamorph::region::BswRegion;
using metamorph::region::BswScratch;

constexpr std::size_t kMinSampleBytes = 2048;
constexpr auto kIdleBeforeCompaction = std::chrono::milliseconds{1500};
constexpr auto kLongestCompactionWait = std::chrono::seconds{60};
constexpr std::uint64_t kIndexBytes = 1024 * 40;

[[nodiscard]] std::int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

[[nodiscard]] std::int32_t regionXOf(std::int64_t key) { return static_cast<std::int32_t>(key); }

[[nodiscard]] std::int32_t regionZOf(std::int64_t key) { return static_cast<std::int32_t>(key >> 32); }

[[nodiscard]] BswScratch& scratch() {
    thread_local BswScratch held;
    return held;
}

} // namespace

BswChunkStore::BswChunkStore(std::string regionFolder) : folder_(std::move(regionFolder)) {
    dictionary_ = BswDictionary::load(folder_ + "/" + metamorph::region::kBswDictionaryName, kLevel);
    trainSettled_ = dictionary_ != nullptr;
    lastActivityMillis_ = nowMillis();
    worker_ = std::jthread([this](std::stop_token stop) { work(stop); });
}

BswChunkStore::~BswChunkStore() = default;

void BswChunkStore::stop() {
    worker_.request_stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::int64_t BswChunkStore::regionKey(std::int32_t regionX, std::int32_t regionZ) {
    return (static_cast<std::int64_t>(regionX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(regionZ) << 32);
}

std::int64_t BswChunkStore::chunkKey(std::int32_t chunkX, std::int32_t chunkZ) { return regionKey(chunkX, chunkZ); }

std::string BswChunkStore::pathOf(const std::string& folder, std::int32_t regionX, std::int32_t regionZ) {
    return folder + "/r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".bsw";
}

void BswChunkStore::touchActivity() { lastActivityMillis_.store(nowMillis(), std::memory_order_relaxed); }

std::shared_ptr<BswRegion> BswChunkStore::acquire(std::int32_t regionX, std::int32_t regionZ, bool create,
                                                  bool& unreadable) {
    const std::int64_t key = regionKey(regionX, regionZ);
    const std::lock_guard<std::mutex> lock{mutex_};
    if (const auto found = openByKey_.find(key); found != openByKey_.end()) {
        open_.splice(open_.end(), open_, found->second);
        return found->second->region;
    }
    unreadable = broken_.contains(key);
    if (unreadable) {
        return nullptr;
    }
    const std::string path = pathOf(folder_, regionX, regionZ);
    BswOptions options;
    options.level = kLevel;
    options.dictionary = dictionary_;
    options.codec = dictionary_ != nullptr ? BswCodec::ZstdDict : BswCodec::Zstd;
    if (create) {
        wire::io::fileIO::createDirectories(folder_);
    }
    std::unique_ptr<BswRegion> opened = BswRegion::open(path, options, create);
    if (opened == nullptr) {
        if (std::filesystem::exists(path)) {
            broken_.insert(key);
            unreadable = true;
            logger_.error([&] { return "Region " + path + " cannot be opened and will not be overwritten."; });
        }
        return nullptr;
    }
    std::shared_ptr<BswRegion> shared{std::move(opened)};
    open_.push_back(Slot{key, shared});
    openByKey_[key] = std::prev(open_.end());
    while (open_.size() > kMaxOpenRegions) {
        openByKey_.erase(open_.front().key);
        open_.pop_front();
    }
    return shared;
}

const BswChunkStore::Pending* BswChunkStore::pendingLocked(std::int32_t chunkX, std::int32_t chunkZ) const {
    const auto key = regionKey(chunkX >> 5, chunkZ >> 5);
    const auto local = static_cast<std::uint32_t>(((chunkZ & 31) << 5) | (chunkX & 31));
    for (const auto* source : {&pending_, &flushing_}) {
        if (const auto region = source->find(key); region != source->end()) {
            if (const auto chunk = region->second.find(local); chunk != region->second.end()) {
                return &chunk->second;
            }
        }
    }
    return nullptr;
}

void BswChunkStore::cacheLocked(std::int64_t key, const std::vector<std::uint8_t>& nbt) {
    const auto size = static_cast<std::int64_t>(nbt.size());
    if (size > kCacheBytes / 4) {
        return;
    }
    if (const auto found = cacheByKey_.find(key); found != cacheByKey_.end()) {
        cacheBytes_ -= static_cast<std::int64_t>(found->second->nbt.size());
        cache_.erase(found->second);
        cacheByKey_.erase(found);
    }
    cache_.push_back(Cached{key, nbt});
    cacheByKey_[key] = std::prev(cache_.end());
    cacheBytes_ += size;
    while (cacheBytes_ > kCacheBytes && !cache_.empty()) {
        cacheBytes_ -= static_cast<std::int64_t>(cache_.front().nbt.size());
        cacheByKey_.erase(cache_.front().key);
        cache_.pop_front();
    }
}

BswChunkStore::Read BswChunkStore::read(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t>& out) {
    touchActivity();
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        if (const Pending* staged = pendingLocked(chunkX, chunkZ); staged != nullptr) {
            if (staged->nbt.empty()) {
                return Read::Absent;
            }
            out.assign(staged->nbt.begin(), staged->nbt.end());
            return Read::Ok;
        }
        if (const auto found = cacheByKey_.find(key); found != cacheByKey_.end()) {
            cache_.splice(cache_.end(), cache_, found->second);
            out.assign(found->second->nbt.begin(), found->second->nbt.end());
            return Read::Ok;
        }
    }
    bool unreadable = false;
    const std::shared_ptr<BswRegion> region = acquire(chunkX >> 5, chunkZ >> 5, false, unreadable);
    if (region == nullptr) {
        return unreadable ? Read::Unreadable : Read::Absent;
    }
    switch (region->read(chunkX & 31, chunkZ & 31, scratch(), out)) {
    case BswRead::Ok: {
        const std::lock_guard<std::mutex> lock{mutex_};
        cacheLocked(key, out);
        return Read::Ok;
    }
    case BswRead::Absent:
        return Read::Absent;
    case BswRead::Corrupt:
        break;
    }
    return Read::Corrupt;
}

bool BswChunkStore::has(std::int32_t chunkX, std::int32_t chunkZ) {
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        if (const Pending* staged = pendingLocked(chunkX, chunkZ); staged != nullptr) {
            return !staged->nbt.empty();
        }
    }
    bool unreadable = false;
    const std::shared_ptr<BswRegion> region = acquire(chunkX >> 5, chunkZ >> 5, false, unreadable);
    return region != nullptr && region->hasChunk(chunkX & 31, chunkZ & 31);
}

void BswChunkStore::stage(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> nbt,
                          std::int32_t timestamp) {
    touchActivity();
    const std::lock_guard<std::mutex> lock{mutex_};
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    if (const auto found = cacheByKey_.find(key); found != cacheByKey_.end()) {
        cacheBytes_ -= static_cast<std::int64_t>(found->second->nbt.size());
        cache_.erase(found->second);
        cacheByKey_.erase(found);
    }
    if (dictionary_ == nullptr && !trainSettled_ && !trainRequested_ && nbt.size() >= kMinSampleBytes) {
        samples_.push_back(nbt);
        if (samples_.size() >= kSampleChunks) {
            trainRequested_ = true;
            wake_.notify_all();
        }
    }
    PendingRegion& region = pending_[regionKey(chunkX >> 5, chunkZ >> 5)];
    const auto local = static_cast<std::uint32_t>(((chunkZ & 31) << 5) | (chunkX & 31));
    pendingBytes_ += static_cast<std::int64_t>(nbt.size());
    if (const auto found = region.find(local); found != region.end()) {
        pendingBytes_ -= static_cast<std::int64_t>(found->second.nbt.size());
    }
    region[local] = Pending{std::move(nbt), timestamp};
}

std::size_t BswChunkStore::flush(bool parallel) {
    const std::lock_guard<std::mutex> flushing{flushMutex_};
    std::vector<std::int64_t> keys;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        if (pending_.empty()) {
            return 0;
        }
        flushing_ = std::move(pending_);
        pending_.clear();
        pendingBytes_ = 0;
        for (const auto& entry : flushing_) {
            keys.push_back(entry.first);
        }
    }
    std::atomic<std::size_t> written{0};
    std::vector<std::int64_t> failed(keys.size(), -1);
    std::vector<std::int64_t> wantsCompaction;
    std::mutex wantsMutex;
    parallelFor(keys.size(), parallel, [&](std::size_t at) {
        const std::int64_t key = keys[at];
        const PendingRegion& staged = flushing_.at(key);
        bool unreadable = false;
        const std::shared_ptr<BswRegion> region = acquire(regionXOf(key), regionZOf(key), true, unreadable);
        std::vector<metamorph::region::BswPut> puts;
        puts.reserve(staged.size());
        for (const auto& [local, chunk] : staged) {
            puts.push_back({static_cast<std::int32_t>(local & 31), static_cast<std::int32_t>(local >> 5), chunk.nbt,
                            chunk.timestamp});
        }
        if (region != nullptr && region->write(puts, scratch())) {
            ++written;
            const metamorph::region::BswStats stats = region->stats();
            if (stats.freeBytes * 4 > stats.fileBytes) {
                const std::lock_guard<std::mutex> lock{wantsMutex};
                wantsCompaction.push_back(key);
            }
        } else if (region != nullptr) {
            failed[at] = key;
            logger_.error([&] {
                return "Could not write region " + pathOf(folder_, regionXOf(key), regionZOf(key)) +
                       "; its chunks stay in memory and are retried at the next save.";
            });
        }
    });
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        for (const std::int64_t key : failed) {
            if (key == -1) {
                continue;
            }
            for (auto& [local, chunk] : flushing_.at(key)) {
                pendingBytes_ += static_cast<std::int64_t>(chunk.nbt.size());
                pending_[key].try_emplace(local, std::move(chunk));
            }
        }
        flushing_.clear();
        compactQueue_.insert(wantsCompaction.begin(), wantsCompaction.end());
    }
    if (!wantsCompaction.empty()) {
        wake_.notify_all();
    }
    return written.load();
}

std::vector<std::int32_t> BswChunkStore::stored(std::int32_t regionX, std::int32_t regionZ) {
    std::vector<std::int32_t> found;
    bool unreadable = false;
    const std::shared_ptr<BswRegion> region = acquire(regionX, regionZ, false, unreadable);
    for (std::int32_t localZ = 0; localZ < 32; ++localZ) {
        for (std::int32_t localX = 0; localX < 32; ++localX) {
            const std::int32_t chunkX = (regionX << 5) + localX;
            const std::int32_t chunkZ = (regionZ << 5) + localZ;
            bool present = region != nullptr && region->hasChunk(localX, localZ);
            {
                const std::lock_guard<std::mutex> lock{mutex_};
                if (const Pending* staged = pendingLocked(chunkX, chunkZ); staged != nullptr) {
                    present = !staged->nbt.empty();
                }
            }
            if (present) {
                found.push_back(chunkX);
                found.push_back(chunkZ);
            }
        }
    }
    return found;
}

std::int64_t BswChunkStore::residentBytes() {
    const std::lock_guard<std::mutex> lock{mutex_};
    return static_cast<std::int64_t>(open_.size() * kIndexBytes) + cacheBytes_ + pendingBytes_;
}

std::size_t BswChunkStore::openRegions() {
    const std::lock_guard<std::mutex> lock{mutex_};
    return open_.size();
}

bool BswChunkStore::hasDictionary() {
    const std::lock_guard<std::mutex> lock{mutex_};
    return dictionary_ != nullptr;
}

void BswChunkStore::trainDictionary(std::vector<std::vector<std::uint8_t>> samples) {
    std::vector<std::span<const std::uint8_t>> views(samples.begin(), samples.end());
    std::shared_ptr<const BswDictionary> trained = BswDictionary::train(views, kDictionaryBytes, kLevel);
    const bool saved = trained != nullptr && trained->save(folder_ + "/" + metamorph::region::kBswDictionaryName);
    std::vector<std::shared_ptr<BswRegion>> held;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        trainSettled_ = true;
        trainRequested_ = false;
        if (!saved) {
            logger_.warn([] { return std::string{"Could not train a chunk dictionary; chunks stay without one."}; });
            return;
        }
        dictionary_ = trained;
        for (const Slot& slot : open_) {
            held.push_back(slot.region);
        }
    }
    for (const auto& region : held) {
        region->useDictionary(trained);
    }
    logger_.info([&] {
        return "Trained a " + std::to_string(trained->bytes().size() / 1024) + " KiB chunk dictionary from " +
               std::to_string(samples.size()) + " chunks.";
    });
}

void BswChunkStore::compactIdle(std::stop_token stop) {
    const auto begun = std::chrono::steady_clock::now();
    std::mutex idle;
    std::unique_lock<std::mutex> lock{idle};
    while (!stop.stop_requested()) {
        const auto quiet = std::chrono::milliseconds{nowMillis() - lastActivityMillis_.load()};
        if (quiet >= kIdleBeforeCompaction || std::chrono::steady_clock::now() - begun > kLongestCompactionWait) {
            return;
        }
        wake_.wait_for(lock, stop, std::chrono::milliseconds{250}, [] { return false; });
    }
}

void BswChunkStore::work(std::stop_token stop) {
    wire::io::lowerCurrentThreadPriority();
    std::unique_lock<std::mutex> lock{mutex_};
    while (!stop.stop_requested()) {
        wake_.wait(lock, stop, [this] { return trainRequested_ || !compactQueue_.empty(); });
        if (stop.stop_requested()) {
            return;
        }
        if (trainRequested_) {
            std::vector<std::vector<std::uint8_t>> samples = std::move(samples_);
            samples_.clear();
            lock.unlock();
            trainDictionary(std::move(samples));
            lock.lock();
            continue;
        }
        lock.unlock();
        compactIdle(stop);
        lock.lock();
        if (stop.stop_requested() || compactQueue_.empty()) {
            continue;
        }
        const std::int64_t key = *compactQueue_.begin();
        compactQueue_.erase(compactQueue_.begin());
        lock.unlock();
        bool unreadable = false;
        const std::shared_ptr<BswRegion> region = acquire(regionXOf(key), regionZOf(key), false, unreadable);
        if (region != nullptr) {
            const metamorph::region::BswStats before = region->stats();
            if (before.freeBytes * 4 > before.fileBytes && region->compact(scratch())) {
                logger_.debug([&] {
                    return "Compacted region " + std::to_string(regionXOf(key)) + "." + std::to_string(regionZOf(key)) +
                           ": " + std::to_string(before.fileBytes / 1024) + " KiB to " +
                           std::to_string(region->stats().fileBytes / 1024) + " KiB.";
                });
            }
        }
        lock.lock();
    }
}

} // namespace basalt::server::runtime::world

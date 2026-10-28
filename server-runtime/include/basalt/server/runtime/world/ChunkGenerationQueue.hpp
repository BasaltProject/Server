#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
class ProtoChunks;
} // namespace basalt::server::world::terrain

namespace basalt::server::runtime::world {

/// Fixed pool pulling from a queue ordered by distance to a player; workers poll so shutdown is one flag.
class ChunkGenerationQueue {
public:
    [[nodiscard]] static std::int32_t defaultWorkers();

    static constexpr std::size_t kCapacity = 4096;

    /// Bounded so a lost worker cannot keep the server from ever finishing startup.
    static constexpr std::int64_t kAwaitTimeoutMillis = 60000;

    ChunkGenerationQueue(WorldState& world, std::string dimension, std::int32_t workerCount = defaultWorkers(),
                         std::int64_t awaitTimeoutMillis = kAwaitTimeoutMillis);

    ChunkGenerationQueue(const ChunkGenerationQueue&) = delete;
    ChunkGenerationQueue& operator=(const ChunkGenerationQueue&) = delete;

    ~ChunkGenerationQueue();

    [[nodiscard]] const std::string& dimension() const noexcept { return dimension_; }

    void usePipeline(server::world::terrain::ProtoChunks* pipeline) noexcept { pipeline_ = pipeline; }

    [[nodiscard]] static std::int64_t pack(std::int32_t chunkX, std::int32_t chunkZ);
    [[nodiscard]] static std::int32_t unpackX(std::int64_t key);
    [[nodiscard]] static std::int32_t unpackZ(std::int64_t key);

    void request(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t priority);

    /// parity: `TicketType.FORCED`, the chunk and two rings full and decorated out to five.
    void requestForced(std::int32_t chunkX, std::int32_t chunkZ);

    void releaseForced(std::int32_t chunkX, std::int32_t chunkZ);

    static constexpr std::int32_t kForcedFullRings = 2;

    /// Drops pending requests `shouldKeep` rejects: a player who moved on leaves requests nobody will look at.
    std::size_t retainWhere(const std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>& shouldKeep);

    [[nodiscard]] std::size_t pendingCount() const;

    [[nodiscard]] std::unordered_set<std::int64_t> pendingKeys() const;

    std::vector<server::world::Chunk*> awaitAll(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions);

    bool generateNext();

    /// Workers finish their current chunk, so pair this with `awaitIdle`.
    void stop();
    void joinWorkers();

    /// Shutdown saves what is in memory, so it must wait for the last chunk to land.
    void awaitIdle(std::int64_t timeoutMillis);

private:
    [[nodiscard]] std::optional<std::int64_t> takeNext();
    void runWorker();

    wire::common::Logger logger_{"ChunkGen"};
    std::int64_t awaitTimeoutMillis_;
    WorldState* world_;
    std::string dimension_;

    /// Lets an install that outlives this queue learn, under a lock, that there is nobody to report to.
    struct Live {
        std::mutex mutex;
        ChunkGenerationQueue* queue = nullptr;
    };

    void markInstalled(std::int64_t key);

    bool generatePiped();

    server::world::terrain::ProtoChunks* pipeline_ = nullptr;
    std::shared_ptr<Live> live_ = std::make_shared<Live>();
    mutable std::mutex mutex_;
    std::unordered_map<std::int64_t, std::int32_t> queued_;
    std::unordered_map<std::int64_t, std::int32_t> inProgress_;
    std::atomic<bool> running_{true};
    std::vector<std::jthread> workers_;
};

} // namespace basalt::server::runtime::world

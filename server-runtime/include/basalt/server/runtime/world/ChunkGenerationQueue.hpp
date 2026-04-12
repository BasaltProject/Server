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

/// The one place chunks get generated off the requesting thread: a fixed pool pulling from a queue
/// ordered by distance to a player. Per dimension, the seam a regional scheduler would shard on.
/// Workers poll rather than wait, which keeps shutdown to one flag.
class ChunkGenerationQueue {
public:
    /// Cores minus one for the tick loop and one for the connection threads.
    [[nodiscard]] static std::int32_t defaultWorkers();

    // Comfortably more than one player's view plus prefetch, and small enough that scanning it to
    // pick the next chunk stays cheap.
    static constexpr std::size_t kCapacity = 4096;

    /// How long `awaitAll` waits for the pool before generating the rest itself. Bounded because a
    /// lost worker would otherwise be a server that never finishes starting, and injectable because
    /// a test with no pool at all should not sit out the whole minute.
    static constexpr std::int64_t kAwaitTimeoutMillis = 60000;

    ChunkGenerationQueue(WorldState& world, std::string dimension, std::int32_t workerCount = defaultWorkers(),
                         std::int64_t awaitTimeoutMillis = kAwaitTimeoutMillis);

    ChunkGenerationQueue(const ChunkGenerationQueue&) = delete;
    ChunkGenerationQueue& operator=(const ChunkGenerationQueue&) = delete;

    /// Stops the workers and waits for the one still generating, so nothing writes into a world
    /// that is going away.
    ~ChunkGenerationQueue();

    [[nodiscard]] const std::string& dimension() const noexcept { return dimension_; }

    /// Hands generation to the chunk pipeline: a request becomes a demand on it, and a worker's
    /// turn is one of its steps. Set once, before the first request.
    void usePipeline(server::world::terrain::ProtoChunks* pipeline) noexcept { pipeline_ = pipeline; }

    [[nodiscard]] static std::int64_t pack(std::int32_t chunkX, std::int32_t chunkZ);
    [[nodiscard]] static std::int32_t unpackX(std::int64_t key);
    [[nodiscard]] static std::int32_t unpackZ(std::int64_t key);

    /// Enqueues this position unless it is already loaded, queued or being generated. Lower
    /// `priority` runs first.
    void request(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t priority);

    /// Forgets pending requests `shouldKeep` rejects, answering how many. A player who has moved on
    /// leaves behind requests for ground nobody will look at again; generating them costs the same
    /// as real work and their results are retained until the next unload sweep notices.
    std::size_t retainWhere(const std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>& shouldKeep);

    /// Pending and in-flight counts, for the diagnostics that report where the server's work goes.
    [[nodiscard]] std::size_t pendingCount() const;

    /// Everything queued or in flight, as one snapshot. Callers ask once and then test against the
    /// result, rather than calling in per chunk: the unload sweep does that test while holding the
    /// world's own lock, and taking this one inside it, hundreds of times, starved the sweep out
    /// entirely against eight workers looping on both locks.
    [[nodiscard]] std::unordered_set<std::int64_t> pendingKeys() const;

    /// Generates every one of these positions and answers them in the same order, blocking until
    /// they are all ready. For the paths that genuinely cannot proceed without the chunks in hand:
    /// the startup batch, and a player's own initial view on login.
    std::vector<server::world::Chunk*> awaitAll(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions);

    /// Generates the next chunk in the queue, answering whether there was one. What a worker does
    /// on every turn, and the seam that lets the queue's own order be checked without threads.
    bool generateNext();

    /// Stops accepting work. Workers finish whatever chunk they are on, so pair this with
    /// `awaitIdle`.
    void stop();

    /// Waits until nothing is being generated, up to `timeoutMillis`. Shutdown saves what is in
    /// memory, so it has to happen after the last chunk lands rather than racing it.
    void awaitIdle(std::int64_t timeoutMillis);

private:
    [[nodiscard]] std::optional<std::int64_t> takeNext();
    void runWorker();

    wire::common::Logger logger_{"ChunkGen"};
    std::int64_t awaitTimeoutMillis_;
    WorldState* world_;
    std::string dimension_;

    /// Lets an install that outlives this queue find out, under a lock, that it has nothing to tell.
    /// The install runs on the world owner long after the worker that asked for it moved on.
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
    /// Taken, or handed to the world and not installed yet.
    std::unordered_map<std::int64_t, std::int32_t> inProgress_;
    std::atomic<bool> running_{true};
    std::vector<std::jthread> workers_;
};

} // namespace basalt::server::runtime::world

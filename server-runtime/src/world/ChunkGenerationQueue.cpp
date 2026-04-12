#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/world/terrain/ProtoChunks.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::Chunk;

constexpr std::int32_t kFallbackCores = 4;
constexpr std::int32_t kReservedCores = 2;

// Past this, region writes and the send budget limit exploration, not the pool.
constexpr std::int32_t kMaxWorkers = 8;

constexpr std::chrono::milliseconds kIdlePoll{1};
constexpr std::chrono::milliseconds kAwaitPoll{10};
constexpr std::int32_t kIdleSpinsBeforeBackoff = 20;
constexpr std::chrono::milliseconds kIdleBackoff{20};
constexpr std::chrono::milliseconds kBetweenChunks{1};

// A forceload asks for every chunk with priority 0, so most picks are ties. Left to the map they
// would go by hash iteration order, which depends on how the requests happened to interleave.
[[nodiscard]] bool rasterBefore(const std::int64_t key, const std::int64_t other) {
    const std::int32_t x = ChunkGenerationQueue::unpackX(key);
    const std::int32_t otherX = ChunkGenerationQueue::unpackX(other);
    if (x != otherX) {
        return x < otherX;
    }
    return ChunkGenerationQueue::unpackZ(key) < ChunkGenerationQueue::unpackZ(other);
}

} // namespace

std::int32_t ChunkGenerationQueue::defaultWorkers() {
    const std::int32_t cores = static_cast<std::int32_t>(wire::io::cpuCoreCount().value_or(kFallbackCores));
    return std::clamp(cores - kReservedCores, 1, kMaxWorkers);
}

std::int64_t ChunkGenerationQueue::pack(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

std::int32_t ChunkGenerationQueue::unpackX(std::int64_t key) { return static_cast<std::int32_t>(key); }

std::int32_t ChunkGenerationQueue::unpackZ(std::int64_t key) { return static_cast<std::int32_t>(key >> 32); }

ChunkGenerationQueue::ChunkGenerationQueue(WorldState& world, std::string dimension, std::int32_t workerCount,
                                           std::int64_t awaitTimeoutMillis)
    : awaitTimeoutMillis_(awaitTimeoutMillis), world_(&world), dimension_(std::move(dimension)) {
    live_->queue = this;
    workers_.reserve(static_cast<std::size_t>(std::max(workerCount, 0)));
    for (std::int32_t worker = 0; worker < workerCount; ++worker) {
        workers_.emplace_back([this] { runWorker(); });
    }
}

void ChunkGenerationQueue::markInstalled(const std::int64_t key) {
    const std::lock_guard<std::mutex> lock{mutex_};
    inProgress_.erase(key);
}

ChunkGenerationQueue::~ChunkGenerationQueue() {
    {
        const std::lock_guard<std::mutex> lock{live_->mutex};
        live_->queue = nullptr;
    }
    stop();
    // The Kotlin let its threads outlive the queue: nothing here can, because the world they write
    // into is destroyed right after this.
    workers_.clear();
}

void ChunkGenerationQueue::runWorker() {
    std::int32_t idleSpins = 0;
    while (running_.load()) {
        bool generated = false;
        // Caught, or one bad chunk ends this worker and the pool silently shrinks.
        try {
            generated = generateNext();
        } catch (const std::exception& failure) {
            logger_.error([&failure] { return std::string{"A chunk failed to generate: "} + failure.what(); });
        }
        if (!generated) {
            // Back off while there is nothing to do. Eight workers each grabbing the lock every
            // millisecond is thousands of acquisitions a second spent discovering the queue is
            // empty, and it is contention everything else then has to win against.
            ++idleSpins;
            wire::io::sleepFor(idleSpins > kIdleSpinsBeforeBackoff ? kIdleBackoff : kIdlePoll);
            continue;
        }
        idleSpins = 0;
        // A deliberate breath between chunks. These eight threads otherwise hold the world's lock
        // in a tight loop, and the single thread that saves, unloads and sends keep-alives never
        // wins against them.
        wire::io::sleepFor(kBetweenChunks);
    }
}

void ChunkGenerationQueue::request(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t priority) {
    const std::int64_t key = pack(chunkX, chunkZ);
    // A queue that has been stopped takes nothing, which the frozen Kotlin left to the workers
    // noticing: one still between its own check and the queue would pick the request up and
    // generate into a world that is being torn down.
    if (!running_.load() || world_->isLoaded(chunkX, chunkZ)) {
        return;
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    if (inProgress_.contains(key)) {
        return;
    }
    if (pipeline_ != nullptr) {
        if (pipeline_->pendingCount() < kCapacity) {
            pipeline_->demand(chunkX, chunkZ, priority);
        }
        return;
    }
    if (const auto existing = queued_.find(key); existing != queued_.end()) {
        // A closer player asking for the same chunk should move it up, never back.
        if (priority < existing->second) {
            existing->second = priority;
        }
        return;
    }
    if (queued_.size() >= kCapacity) {
        // Requests arrive faster than chunks can be generated whenever a player moves at any speed,
        // so without a ceiling this grows without bound, and every extra entry makes picking the
        // next one slower, which makes it grow faster still. Past the ceiling the farthest pending
        // request is the one worth losing.
        // Ties as in takeNext: among equals the one that decorates last is the one dropped.
        const auto worst = std::ranges::max_element(queued_, [](const auto& left, const auto& right) {
            if (left.second != right.second) {
                return left.second < right.second;
            }
            return rasterBefore(right.first, left.first);
        });
        if (worst == queued_.end() || worst->second <= priority) {
            return;
        }
        queued_.erase(worst);
    }
    queued_.emplace(key, priority);
}

std::size_t
ChunkGenerationQueue::retainWhere(const std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>& shouldKeep) {
    if (pipeline_ != nullptr) {
        return pipeline_->retainWhere(shouldKeep);
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    std::size_t forgotten = 0;
    for (auto at = queued_.begin(); at != queued_.end();) {
        if (shouldKeep(unpackX(at->first), unpackZ(at->first))) {
            ++at;
            continue;
        }
        at = queued_.erase(at);
        ++forgotten;
    }
    return forgotten;
}

std::size_t ChunkGenerationQueue::pendingCount() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return queued_.size() + inProgress_.size() + (pipeline_ != nullptr ? pipeline_->pendingCount() : 0);
}

std::unordered_set<std::int64_t> ChunkGenerationQueue::pendingKeys() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::unordered_set<std::int64_t> keys;
    for (const auto& [key, priority] : inProgress_) {
        keys.insert(key);
    }
    for (const auto& [key, priority] : queued_) {
        keys.insert(key);
    }
    if (pipeline_ != nullptr) {
        keys.merge(pipeline_->pendingKeys());
    }
    return keys;
}

std::vector<server::world::Chunk*>
ChunkGenerationQueue::awaitAll(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) {
    if (positions.empty()) {
        return {};
    }
    for (const auto& [chunkX, chunkZ] : positions) {
        request(chunkX, chunkZ, 0);
    }
    // One lock acquisition per poll, not one per position: asking about each position on its own
    // was hundreds of thousands of acquisitions a second. Past the deadline the rest are generated
    // on this thread.
    std::int64_t waited = 0;
    while (world_->countMissing(positions) > 0 && waited < awaitTimeoutMillis_) {
        // Waiting on the owner's own thread would wait for ever: the workers hand their chunks to
        // it, so it has to keep running what it is handed while it waits. That is the one reentrant
        // point in the model, and it is here rather than anywhere a task can reach.
        if (sched::Owner* owner = world_->owner(); owner != nullptr && owner->isCurrent()) {
            static_cast<void>(owner->runPending());
        }
        wire::io::sleepFor(kAwaitPoll);
        waited += kAwaitPoll.count();
    }
    std::vector<Chunk*> ready;
    ready.reserve(positions.size());
    for (const auto& [chunkX, chunkZ] : positions) {
        ready.push_back(&world_->getOrGenerateChunk(chunkX, chunkZ));
    }
    return ready;
}

void ChunkGenerationQueue::stop() {
    running_.store(false);
    const std::lock_guard<std::mutex> lock{mutex_};
    queued_.clear();
    if (pipeline_ != nullptr) {
        pipeline_->clearDemands();
    }
}

void ChunkGenerationQueue::awaitIdle(std::int64_t timeoutMillis) {
    std::int64_t waited = 0;
    while (waited < timeoutMillis) {
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            if (inProgress_.empty() && (pipeline_ == nullptr || !pipeline_->busy())) {
                return;
            }
        }
        wire::io::sleepFor(kIdlePoll);
        waited += kIdlePoll.count();
    }
}

std::optional<std::int64_t> ChunkGenerationQueue::takeNext() {
    const std::lock_guard<std::mutex> lock{mutex_};
    if (queued_.empty()) {
        return std::nullopt;
    }
    std::int64_t bestKey = 0;
    std::int32_t bestPriority = std::numeric_limits<std::int32_t>::max();
    bool found = false;
    for (const auto& [key, priority] : queued_) {
        if (priority > bestPriority) {
            continue;
        }
        if (found && priority == bestPriority && !rasterBefore(key, bestKey)) {
            continue;
        }
        bestPriority = priority;
        bestKey = key;
        found = true;
    }
    if (!found) {
        return std::nullopt;
    }
    queued_.erase(bestKey);
    inProgress_.emplace(bestKey, bestPriority);
    return bestKey;
}

bool ChunkGenerationQueue::generatePiped() {
    const bool worked = pipeline_->step();
    std::vector<Chunk> finished;
    {
        // Under the lock with the keys marked, so a request between the two cannot find the chunk
        // neither in the pipeline nor on its way in, and ask for it all over again.
        const std::lock_guard<std::mutex> lock{mutex_};
        finished = pipeline_->takeFinished();
        for (const Chunk& chunk : finished) {
            inProgress_.emplace(pack(chunk.chunkX(), chunk.chunkZ()), 0);
        }
    }
    for (Chunk& chunk : finished) {
        const std::int64_t key = pack(chunk.chunkX(), chunk.chunkZ());
        world_->installLater(std::move(chunk), [live = live_, key]() {
            const std::lock_guard<std::mutex> lock{live->mutex};
            if (live->queue != nullptr) {
                live->queue->markInstalled(key);
            }
        });
    }
    return worked || !finished.empty();
}

bool ChunkGenerationQueue::generateNext() {
    if (pipeline_ != nullptr) {
        return generatePiped();
    }
    const std::optional<std::int64_t> next = takeNext();
    if (!next.has_value()) {
        return false;
    }
    const std::int64_t key = *next;

    // A chunk stays in progress until it is really in the world, not until it is generated:
    // installing is a task the world owner runs later, and a request in between would make it twice.
    struct Finally {
        ChunkGenerationQueue* queue;
        std::int64_t key;
        bool handedOff = false;

        ~Finally() {
            if (handedOff) {
                return;
            }
            const std::lock_guard<std::mutex> lock{queue->mutex_};
            queue->inProgress_.erase(key);
        }
    } finish{this, key};

    // Generated here, installed by whoever owns the world: the set of chunks is that owner's state,
    // and a worker that put its own chunk in it was the last thing keeping the world's lock in the
    // hot path. A freshly generated chunk marks itself dirty as it is filled in, so persistence is
    // still driven from the chunks themselves.
    std::optional<Chunk> chunk = world_->generateDetached(unpackX(key), unpackZ(key));
    if (!chunk.has_value()) {
        return true;
    }
    finish.handedOff = true;
    world_->installLater(*std::move(chunk), [live = live_, key]() {
        const std::lock_guard<std::mutex> lock{live->mutex};
        if (live->queue != nullptr) {
            live->queue->markInstalled(key);
        }
    });
    return true;
}

} // namespace basalt::server::runtime::world

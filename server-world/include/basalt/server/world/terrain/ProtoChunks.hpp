#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace basalt::server::world::terrain {

class ProtoRegion;

/// parity: `ChunkPyramid.GENERATION_PYRAMID` from `TERRAIN` to `FULL`: a chunk decorates once its eight
/// neighbours have terrain, and is full once those eight have decorated too.
/// Overlapping decorations never run together and wait in a fixed order (priority, x, z), so the same
/// requests make the same world. Any number of threads may call `step` at once.
class ProtoChunks {
public:
    struct Stored {
        Chunk chunk;
        ChunkStage stage = ChunkStage::Full;
    };

    struct Hooks {
        /// What the region files hold for this chunk, at whatever stage it was saved.
        std::function<std::optional<Stored>(std::int32_t, std::int32_t)> load;
        std::function<realTerrainGenerator::GeneratedTerrain(std::int32_t, std::int32_t)> terrain;
        /// Wraps a proto chunk read back from disk so it can decorate.
        std::function<realTerrainGenerator::GeneratedTerrain(Chunk)> adopt;
        std::function<void(realTerrainGenerator::GeneratedTerrain&, ProtoRegion&)> decorate;
        /// Runs on a chunk as it leaves: `generated` is false for one that came off disk already full.
        std::function<void(Chunk&, bool generated)> finish;
        /// Keeps proto chunks that have to leave memory before they are full. Each carries its stage.
        std::function<void(std::vector<Chunk>)> persist;
    };

    /// Ahead of any request, for a caller that cannot go on without the chunk.
    static constexpr std::int32_t kImmediate = std::numeric_limits<std::int32_t>::min();

    // Proto chunks kept around the full ones before the oldest spare ones go to disk.
    static constexpr std::size_t kDefaultCapacity = 1024;

    explicit ProtoChunks(Hooks hooks, std::size_t capacity = kDefaultCapacity);
    ~ProtoChunks();
    ProtoChunks(const ProtoChunks&) = delete;
    ProtoChunks& operator=(const ProtoChunks&) = delete;
    ProtoChunks(ProtoChunks&&) = delete;
    ProtoChunks& operator=(ProtoChunks&&) = delete;

    /// Asks for this chunk full. Lower priority first; asking again can only move it up.
    void demand(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t priority);

    /// Forgets the requests `keep` rejects, answering how many.
    std::size_t retainWhere(const std::function<bool(std::int32_t, std::int32_t)>& keep);

    /// Runs one piece of work, answering whether there was any this thread could take.
    bool step();

    /// The chunks finished for a request, handed over once.
    [[nodiscard]] std::vector<Chunk> takeFinished();

    /// Makes this chunk full on the calling thread, helping with whatever it waits on. Nothing when
    /// it was already finished and handed over, so the caller should find it in the world.
    [[nodiscard]] std::optional<Chunk> generateNow(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::unordered_set<std::int64_t> pendingKeys() const;

    [[nodiscard]] std::size_t pendingCount() const;

    /// Proto chunks held in memory, for the diagnostics.
    [[nodiscard]] std::size_t protoCount() const;

    /// Drops every request, so the pool runs dry.
    void clearDemands();

    /// Whether some thread is working on a piece right now.
    [[nodiscard]] bool busy() const;

    /// Hands every proto chunk that cannot be rebuilt from the seed to `persist`, for a shutdown.
    void persistAll();

    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

private:
    struct Entry;
    struct Request;
    struct Task;

    void refreshWants();
    [[nodiscard]] std::optional<Task> pick();
    [[nodiscard]] bool decorationMayRun(std::int64_t chunk, const std::unordered_set<std::int64_t>& earlier) const;
    void runTask(Task& task);
    [[nodiscard]] std::vector<Chunk> evict();
    void persistOutside(std::vector<Chunk> chunks);

    Hooks hooks_;
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<std::int64_t, std::unique_ptr<Entry>> entries_;
    std::unordered_map<std::int64_t, Request> requests_;
    /// Chunks that went full here, so asking for one again means loading it rather than making it.
    std::unordered_set<std::int64_t> full_;
    std::unordered_set<std::int64_t> busy_;
    std::unordered_set<std::int64_t> decorating_;
    std::unordered_set<std::int64_t> persisting_;
    std::unordered_map<std::int64_t, std::int32_t> decorationWants_;
    std::unordered_map<std::int64_t, std::int32_t> terrainWants_;
    std::vector<std::int64_t> decorationOrder_;
    std::vector<std::int64_t> unresolved_;
    bool wantsStale_ = true;
    std::uint64_t clock_ = 0;

    struct Finished {
        std::int64_t key = 0;
        Chunk chunk;
        bool waited = false;
    };

    std::vector<Finished> finished_;
};

} // namespace basalt::server::world::terrain

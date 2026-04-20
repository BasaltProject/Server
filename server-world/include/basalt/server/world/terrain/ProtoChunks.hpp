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

/// parity: `ChunkPyramid.GENERATION_PYRAMID` from `TERRAIN` to `FULL`.
class ProtoChunks {
public:
    struct Stored {
        Chunk chunk;
        ChunkStage stage = ChunkStage::Full;
    };

    struct Hooks {
        std::function<std::optional<Stored>(std::int32_t, std::int32_t)> load;
        std::function<realTerrainGenerator::GeneratedTerrain(std::int32_t, std::int32_t)> terrain;
        std::function<realTerrainGenerator::GeneratedTerrain(Chunk)> adopt;
        std::function<void(realTerrainGenerator::GeneratedTerrain&, ProtoRegion&)> decorate;
        std::function<void(Chunk&, bool generated)> finish;
        std::function<void(std::vector<Chunk>)> persist;
    };

    static constexpr std::int32_t kImmediate = std::numeric_limits<std::int32_t>::min();

    static constexpr std::size_t kDefaultCapacity = 1024;

    explicit ProtoChunks(Hooks hooks, std::size_t capacity = kDefaultCapacity);
    ~ProtoChunks();
    ProtoChunks(const ProtoChunks&) = delete;
    ProtoChunks& operator=(const ProtoChunks&) = delete;
    ProtoChunks(ProtoChunks&&) = delete;
    ProtoChunks& operator=(ProtoChunks&&) = delete;

    void demand(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t priority);

    /// parity: `ChunkMap.FORCED_TICKET_LEVEL`, the chunk and `fullRadius` rings full, decorated out to `decorateRadius`.
    void demandArea(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t fullRadius, std::int32_t decorateRadius,
                    std::int32_t priority);

    void releaseArea(std::int32_t chunkX, std::int32_t chunkZ);

    std::size_t retainWhere(const std::function<bool(std::int32_t, std::int32_t)>& keep);

    bool step();

    [[nodiscard]] std::vector<Chunk> takeFinished();

    [[nodiscard]] std::optional<Chunk> generateNow(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::unordered_set<std::int64_t> pendingKeys() const;

    [[nodiscard]] std::size_t pendingCount() const;

    [[nodiscard]] std::size_t protoCount() const;

    void clearDemands();

    [[nodiscard]] bool busy() const;

    void persistAll();

    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

private:
    struct Entry;
    struct Request;

    struct Area {
        std::int32_t priority = 0;
        std::int32_t fullRadius = 0;
        std::int32_t decorateRadius = 0;
    };
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
    std::unordered_map<std::int64_t, Area> areas_;
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

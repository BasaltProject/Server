#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTracker.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace basalt::server::runtime {

class ClientSession;

namespace world {
class AntiXray;
}

class ChunkStreamer {
public:
    explicit ChunkStreamer(ClientSession& owner);

    [[nodiscard]] server::world::ChunkTracker::Delta initialJoin(double x, double z, std::int32_t radius);

    void sendInitialChunks(const server::world::ChunkTracker::Delta& initialChunks);

    void resetForRespawn(double spawnX, double spawnZ);

    void forgetAll();

    void resetForWorldSwitch(double x, double z, std::int32_t radius);

    void resendChunk(std::int32_t chunkX, std::int32_t chunkZ);

    void sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks);

    /// parity: `ServerPlayer.requestedViewDistance`.
    void setRequestedViewDistance(std::int32_t distance);

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> sentChunks() const;

    /// A plugin's own distance for this player, in place of the server's until it is cleared.
    void overrideViewDistance(std::optional<std::int32_t> distance);

    /// parity: `ChunkMap.getPlayerViewDistance`.
    [[nodiscard]] std::int32_t trackedRadius() const;

    void streamChunksAround(double x, double z);

    std::int32_t flushReadyChunks(std::int32_t limit);

    float desiredChunksPerTick;
    float sendBudget = 0.0F;

private:
    [[nodiscard]] bool bypassesAntiXray(const world::AntiXray& antiXray) const;

    /// parity: `ChunkMap.getPlayerViewDistance`'s floor.
    static constexpr std::int32_t kMinViewDistance = 2;

    [[nodiscard]] std::int64_t squaredDistanceToCenter(std::int32_t chunkX, std::int32_t chunkZ) const;

    void applyDelta(const server::world::ChunkTracker::Delta& delta);

    void sendChunk(server::world::Chunk& chunk);

    void prefetchAhead(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t movedX, std::int32_t movedZ);

    [[nodiscard]] bool neighboursLoaded(std::int32_t chunkX, std::int32_t chunkZ);

    ClientSession* owner_;
    server::world::ChunkTracker chunkTracker_;
    std::int32_t viewRadius_ = 0;
    std::optional<std::int32_t> viewOverride_;
    /// parity: `ServerPlayer.requestedViewDistance`, two until the client says otherwise.
    std::int32_t requestedViewDistance_ = kMinViewDistance;
    std::int32_t centerChunkX_ = 0;
    std::int32_t centerChunkZ_ = 0;
    mutable std::mutex chunkStreamMutex_;
    std::vector<server::world::ChunkPos> pendingChunks_;
    std::set<wire::common::Identifier> loggedUnknownBlockEntities_;
};

} // namespace basalt::server::runtime

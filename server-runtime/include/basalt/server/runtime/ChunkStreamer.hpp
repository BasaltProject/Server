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

/// The chunk-streaming state and its packets: view radius, tracker, the queue of chunks generated but
/// not yet sent, and the client's own chunks-per-tick budget. Entering play, respawning and switching
/// worlds each reset it differently, hence one method each.
class ChunkStreamer {
public:
    explicit ChunkStreamer(ClientSession& owner);

    /// The chunk-radius half of a fresh join, seeding the tracker with the view about to be sent.
    [[nodiscard]] server::world::ChunkTracker::Delta initialJoin(double x, double z, std::int32_t radius);

    void sendInitialChunks(const server::world::ChunkTracker::Delta& initialChunks);

    void resetForRespawn(double spawnX, double spawnZ);

    void resetForWorldSwitch(double x, double z, std::int32_t radius);

    /// Sends one chunk again, for a change too big to patch block by block.
    void resendChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// New biomes for these columns without resending the columns themselves.
    void sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks);

    /// parity: `ServerPlayer.requestedViewDistance`, from the client's settings, which narrows the
    /// view to `ChunkMap.getPlayerViewDistance`: between two and the server's own distance.
    void setRequestedViewDistance(std::int32_t distance);

    /// Asks for whatever the player has walked into view of, and forgets what they walked out of.
    void streamChunksAround(double x, double z);

    /// Puts what the generator has finished on the wire, up to this many columns.
    std::int32_t flushReadyChunks(std::int32_t limit);

    /// What the client says it can keep up with, and what is left of this tick's allowance.
    float desiredChunksPerTick;
    float sendBudget = 0.0F;

private:
    /// parity: `ChunkMap.getPlayerViewDistance`'s floor.
    static constexpr std::int32_t kMinViewDistance = 2;

    [[nodiscard]] std::int64_t squaredDistanceToCenter(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// The distance the tracked view is drawn at, which the client's own setting may narrow.
    [[nodiscard]] std::int32_t trackedRadius() const;

    /// Queues what came into view and forgets what left it.
    void applyDelta(const server::world::ChunkTracker::Delta& delta);

    void sendChunk(server::world::Chunk& chunk);

    /// Asks for the ground the player is walking towards before they can see it.
    void prefetchAhead(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t movedX, std::int32_t movedZ);

    /// Whether the eight around this one are loaded, asking for any that are not.
    [[nodiscard]] bool neighboursLoaded(std::int32_t chunkX, std::int32_t chunkZ);

    ClientSession* owner_;
    server::world::ChunkTracker chunkTracker_;
    std::int32_t viewRadius_ = 0;
    /// parity: `ServerPlayer.requestedViewDistance`, two until the client says otherwise.
    std::int32_t requestedViewDistance_ = kMinViewDistance;
    std::int32_t centerChunkX_ = 0;
    std::int32_t centerChunkZ_ = 0;
    /// Requested from the generation queue, not yet on the wire. Both this connection's thread and
    /// the tick loop drain it, so it is guarded.
    mutable std::mutex chunkStreamMutex_;
    std::vector<server::world::ChunkPos> pendingChunks_;
    std::set<wire::common::Identifier> loggedUnknownBlockEntities_;
};

} // namespace basalt::server::runtime

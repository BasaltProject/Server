#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// Where the sculk sensors are, so nothing has to search for them: a footstep happens every tick for
/// every player. Kept as blocks are set and as chunks arrive, and since a sculk sensor always has a
/// block entity a chunk hands over its list rather than being walked block by block.
class SculkSensorTracker {
public:
    explicit SculkSensorTracker(WorldMutex& mutex) : mutex_(&mutex) {}

    /// Called from the block write path without the lock held: the owner is the only writer of
    /// blocks, so this set only ever changes from that one thread.
    void remember(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Every sensor a chunk brought with it, taken from its block entities.
    void rememberChunk(const server::world::Chunk& chunk);

    /// Drops everything inside that chunk, for when it leaves the world.
    void forgetChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// Whether any known sensor is within `range` blocks of this point.
    [[nodiscard]] bool hasNear(double x, double y, double z, std::int32_t range) const;

    [[nodiscard]] std::size_t size() const;

private:
    WorldMutex* mutex_;
    std::vector<std::int64_t> sculkSensors_;
};

} // namespace basalt::server::runtime::world

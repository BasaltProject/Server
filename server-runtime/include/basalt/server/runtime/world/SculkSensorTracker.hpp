#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

class SculkSensorTracker {
public:
    explicit SculkSensorTracker(WorldMutex& mutex) : mutex_(&mutex) {}

    void remember(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    void rememberChunk(const server::world::Chunk& chunk);

    void forgetChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] bool hasNear(double x, double y, double z, std::int32_t range) const;

    [[nodiscard]] std::size_t size() const;

private:
    WorldMutex* mutex_;
    std::vector<std::int64_t> sculkSensors_;
};

} // namespace basalt::server::runtime::world

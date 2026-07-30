#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <array>
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

    /// Sensor positions within `range` blocks on every axis, ordered by y, then z, then x.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> within(std::int32_t x, std::int32_t y, std::int32_t z,
                                                                  std::int32_t range) const;

    void noteRange(std::int32_t range) noexcept;

    [[nodiscard]] std::int32_t widestRange() const noexcept { return widest_; }

    [[nodiscard]] std::size_t size() const;

private:
    WorldMutex* mutex_;
    std::vector<std::int64_t> sculkSensors_;
    std::int32_t widest_ = 0;
};

} // namespace basalt::server::runtime::world

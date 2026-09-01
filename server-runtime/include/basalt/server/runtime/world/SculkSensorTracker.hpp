#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace basalt::server::runtime::world {

/// Where the sculk sensors are, so a walking player does not have to search for them.
///
/// A vibration from a block event can afford to look around itself; a footstep happens every tick
/// for every player, and cannot. Kept as blocks are set, which covers everything placed or broken
/// while the server runs: a sensor that came with the terrain is not in here until something
/// touches it, so footsteps do not reach the deep dark yet.
class SculkSensorTracker {
public:
    explicit SculkSensorTracker(std::mutex& mutex) : mutex_(&mutex) {}

    /// Called from the block write path without the lock held: the owner is the only writer of
    /// blocks, so this set only ever changes from that one thread.
    void remember(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Whether any known sensor is within `range` blocks of this point.
    [[nodiscard]] bool hasNear(double x, double y, double z, std::int32_t range) const;

    [[nodiscard]] std::size_t size() const;

private:
    std::mutex* mutex_;
    std::vector<std::int64_t> sculkSensors_;
};

} // namespace basalt::server::runtime::world

#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace basalt::server::runtime::world {

/// The TNT blocks that were lit this tick and still have to become entities.
///
/// The block itself goes the moment the signal reaches it, but only the caller knows how to
/// allocate an entity id, so what it left behind waits here for one tick at most.
class PrimedTntQueue {
public:
    explicit PrimedTntQueue(std::mutex& mutex) : mutex_(&mutex) {}

    /// parity: `TntBlock.explode`, which spawns the entity where the block stood.
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z);

    /// Takes what was lit, leaving the queue empty.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> takePrimedTnt();

private:
    std::mutex* mutex_;
    std::vector<std::array<std::int32_t, 3>> primedTnt_;
};

} // namespace basalt::server::runtime::world

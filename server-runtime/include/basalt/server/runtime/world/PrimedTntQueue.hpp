#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The TNT blocks that were lit this tick and still have to become entities.
///
/// The block itself goes the moment the signal reaches it, but only the caller knows how to
/// allocate an entity id, so what it left behind waits here for one tick at most.
class PrimedTntQueue {
public:
    explicit PrimedTntQueue(WorldMutex& mutex) : mutex_(&mutex) {}

    /// One square whose tnt was lit, and the game event that lighting it raised.
    ///
    /// parity: the two paths do not raise the same one. `TntBlock.prime` raises `PRIME_FUSE`, and a
    /// dispenser builds the entity itself and raises `ENTITY_PLACE`, so a sculk sensor beside the two
    /// hears different frequencies.
    struct Primed {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::string event;
    };

    /// parity: `TntBlock.explode`, which spawns the entity where the block stood.
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event);

    /// Takes what was lit, leaving the queue empty.
    [[nodiscard]] std::vector<Primed> takePrimedTnt();

private:
    WorldMutex* mutex_;
    std::vector<Primed> primedTnt_;
};

} // namespace basalt::server::runtime::world

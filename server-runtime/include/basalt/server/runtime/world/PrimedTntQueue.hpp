#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class PrimedTntQueue {
public:
    explicit PrimedTntQueue(WorldMutex& mutex) : mutex_(&mutex) {}

    /// parity: `TntBlock.prime`.
    struct Primed {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::string event;
    };

    /// parity: `TntBlock.explode`, which spawns the entity where the block stood.
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event);

    [[nodiscard]] std::vector<Primed> takePrimedTnt();

private:
    WorldMutex* mutex_;
    std::vector<Primed> primedTnt_;
};

} // namespace basalt::server::runtime::world

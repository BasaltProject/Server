#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime::sched {
class Region;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

/// parity: `simulation-distance` in `server.properties`, ten chunks.
inline constexpr std::int32_t kVanillaSimulationDistance = 10;

class ChunkSimulationView {
public:
    explicit ChunkSimulationView(player::PlayerRegistry& players);

    [[nodiscard]] std::vector<std::array<std::int32_t, 2>> simulatedChunks(const world::LoadedWorld& world,
                                                                           const sched::Region* region = nullptr,
                                                                           std::int32_t extraRadius = 0) const;

private:
    player::PlayerRegistry* players_;
};

} // namespace basalt::server::runtime

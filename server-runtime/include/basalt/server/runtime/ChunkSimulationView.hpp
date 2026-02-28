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

/// The chunks each tick method walks that are meant to be simulated, not merely loaded.
class ChunkSimulationView {
public:
    explicit ChunkSimulationView(player::PlayerRegistry& players);

    /// The chunks around whoever is in the world, without repeats.
    ///
    /// parity: vanilla's simulation distance, which is ten by default and separate from the view
    /// distance. This build has no setting of its own for it yet, so it follows the view distance up
    /// to vanilla's own default and no further.
    /// parity: `ChunkLevel.byStatus` puts `BLOCK_TICKING` at level 32 and `ENTITY_TICKING` at 31, so
    /// the blocks of a chunk tick **one further out** than its entities. `extraRadius` is that one.
    [[nodiscard]] std::vector<std::array<std::int32_t, 2>> simulatedChunks(const world::LoadedWorld& world,
                                                                           const sched::Region* region = nullptr,
                                                                           std::int32_t extraRadius = 0) const;

private:
    player::PlayerRegistry* players_;
};

} // namespace basalt::server::runtime

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>
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

    using Chunks = std::vector<std::array<std::int32_t, 2>>;

    /// Shared and immutable: recomputed only when the world, region, players or forced chunks changed.
    [[nodiscard]] std::shared_ptr<const Chunks> simulatedChunks(const world::LoadedWorld& world,
                                                                const sched::Region* region = nullptr,
                                                                std::int32_t extraRadius = 0) const;

    using Keys = std::unordered_set<std::int64_t>;

    /// Same cache as `simulatedChunks`, as packed `(x << 32) | z` keys for membership tests.
    [[nodiscard]] std::shared_ptr<const Keys> simulatedKeys(const world::LoadedWorld& world,
                                                            const sched::Region* region = nullptr,
                                                            std::int32_t extraRadius = 0) const;

private:
    struct Entry {
        const world::LoadedWorld* world;
        const sched::Region* region;
        std::int32_t extraRadius;
        std::uint64_t generation;
        std::int32_t radius;
        std::vector<std::array<std::int32_t, 2>> centres;
        std::vector<std::int64_t> forced;
        std::shared_ptr<const Chunks> result;
        std::shared_ptr<const Keys> keys;
    };

    player::PlayerRegistry* players_;
    mutable std::mutex lock_;
    mutable std::vector<Entry> cache_;
};

} // namespace basalt::server::runtime

#include <basalt/server/world/ChunkTracker.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace basalt::server::world {

bool ChunkTracker::isWithinDistance(std::int32_t centerX, std::int32_t centerZ, std::int32_t viewDistance,
                                    std::int32_t chunkX, std::int32_t chunkZ, bool includeNeighbors) {
    const std::int64_t buffer = includeNeighbors ? 2 : 1;
    const std::int64_t deltaX =
        std::max<std::int64_t>(0, std::abs(static_cast<std::int64_t>(chunkX) - centerX) - buffer);
    const std::int64_t deltaZ =
        std::max<std::int64_t>(0, std::abs(static_cast<std::int64_t>(chunkZ) - centerZ) - buffer);
    return (deltaX * deltaX) + (deltaZ * deltaZ) < static_cast<std::int64_t>(viewDistance) * viewDistance;
}

ChunkTracker::Delta ChunkTracker::update(const std::int32_t centerX, const std::int32_t centerZ,
                                         const std::int32_t radius) {
    if (radius < 0) {
        throw std::invalid_argument{"radius must be non-negative"};
    }

    // parity: `Positioned.forEach`.
    std::set<ChunkPos> desired;
    for (std::int32_t x = centerX - radius - 1; x <= centerX + radius + 1; ++x) {
        for (std::int32_t z = centerZ - radius - 1; z <= centerZ + radius + 1; ++z) {
            if (isWithinDistance(centerX, centerZ, radius, x, z, true)) {
                desired.insert(ChunkPos{.x = x, .z = z});
            }
        }
    }

    Delta delta;
    std::ranges::set_difference(desired, loadedChunks_, std::back_inserter(delta.toLoad));
    std::ranges::set_difference(loadedChunks_, desired, std::back_inserter(delta.toUnload));
    loadedChunks_ = std::move(desired);
    return delta;
}

} // namespace basalt::server::world

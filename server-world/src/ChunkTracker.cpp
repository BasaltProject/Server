#include <basalt/server/world/ChunkTracker.hpp>

#include <algorithm>
#include <stdexcept>

namespace basalt::server::world {

ChunkTracker::Delta ChunkTracker::update(const std::int32_t centerX, const std::int32_t centerZ,
                                         const std::int32_t radius) {
    if (radius < 0) {
        throw std::invalid_argument{"radius must be non-negative"};
    }

    std::set<ChunkPos> desired;
    for (std::int32_t x = centerX - radius; x <= centerX + radius; ++x) {
        for (std::int32_t z = centerZ - radius; z <= centerZ + radius; ++z) {
            desired.insert(ChunkPos{.x = x, .z = z});
        }
    }

    Delta delta;
    std::ranges::set_difference(desired, loadedChunks_, std::back_inserter(delta.toLoad));
    std::ranges::set_difference(loadedChunks_, desired, std::back_inserter(delta.toUnload));
    loadedChunks_ = std::move(desired);
    return delta;
}

} // namespace basalt::server::world

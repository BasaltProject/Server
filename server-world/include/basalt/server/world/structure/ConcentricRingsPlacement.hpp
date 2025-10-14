#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `ConcentricRingsStructurePlacement` plus `generateRingPositions`. The stronghold is the
/// only structure not spread over a grid: a fixed list of chunks on rings around the origin. The
/// whole list is built at once because each position moves the shared random on.
class ConcentricRingsPlacement {
public:
    /// The biome at a quart position, which is what the search samples.
    using NoiseBiomeAt = std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t)>;

    ConcentricRingsPlacement(std::int32_t distance, std::int32_t spread, std::int32_t count,
                             std::set<std::string, std::less<>> preferredBiomes, NoiseBiomeAt noiseBiomeAt)
        : distance_(distance), spread_(spread), count_(count), preferredBiomes_(std::move(preferredBiomes)),
          noiseBiomeAt_(std::move(noiseBiomeAt)) {}

    /// Chunk coordinates, in the order the rings were laid out.
    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> ringPositions(std::int64_t seed) const;

private:
    std::int32_t distance_;
    std::int32_t spread_;
    std::int32_t count_;
    std::set<std::string, std::less<>> preferredBiomes_;
    NoiseBiomeAt noiseBiomeAt_;
};

} // namespace basalt::server::world::structure

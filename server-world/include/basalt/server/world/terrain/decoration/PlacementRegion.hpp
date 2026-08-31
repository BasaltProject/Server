#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

using ExternalHeightReader = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t)>;
using ExternalStateReader = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>;

/// The chunk plus whatever of the surrounding region is readable. Ores and disks are spheres and
/// discs, so most cross a chunk edge: the outside positions still consume random and still have to
/// be written, or a large blob comes out about a third short of vanilla's.
class PlacementRegion {
public:
    PlacementRegion(Chunk& chunk, ExternalHeightReader externalSurfaceHeight, ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalSurfaceHeight_(std::move(externalSurfaceHeight)),
          externalBlockState_(std::move(externalBlockState)) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    /// parity: the preflight asks the level, so it sees blobs this same step already put down. A
    /// copy taken when the region was built would freeze the surface as it was before any ore.
    [[nodiscard]] std::optional<std::int32_t> oceanFloorHeight(std::int32_t x, std::int32_t z) const;

    [[nodiscard]] bool hasOceanFloorAtOrAbove(std::int32_t xStart, std::int32_t zStart, std::int32_t sizeXZ,
                                              std::int32_t yStart) const;

    [[nodiscard]] std::optional<std::int32_t> blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// `replaceable` decides the delivery guard for a deferred write: an ore re-tests its own target
    /// predicate when it lands, so guarding on one exact block would drop every position a
    /// neighbour touched first, and those are all on the border.
    void setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                         const std::set<std::string, std::less<>>* replaceable = nullptr);

private:
    Chunk* chunk_;
    ExternalHeightReader externalSurfaceHeight_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<std::array<std::int32_t, 3>, std::int32_t>> externalWrites_;
};

} // namespace basalt::server::world::terrain::decoration

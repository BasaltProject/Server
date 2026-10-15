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

class PlacementRegion {
public:
    PlacementRegion(Chunk& chunk, ExternalHeightReader externalSurfaceHeight, ExternalStateReader externalBlockState)
        : chunk_(&chunk), externalSurfaceHeight_(std::move(externalSurfaceHeight)),
          externalBlockState_(std::move(externalBlockState)) {}

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    /// parity: the preflight asks the level, so it sees blobs this same step already put down. A.
    [[nodiscard]] std::optional<std::int32_t> oceanFloorHeight(std::int32_t x, std::int32_t z) const;

    [[nodiscard]] bool hasOceanFloorAtOrAbove(std::int32_t xStart, std::int32_t zStart, std::int32_t sizeXZ,
                                              std::int32_t yStart) const;

    [[nodiscard]] std::optional<std::int32_t> blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                         const std::set<std::string, std::less<>>* replaceable = nullptr);

private:
    Chunk* chunk_;
    ExternalHeightReader externalSurfaceHeight_;
    ExternalStateReader externalBlockState_;
    std::vector<std::pair<std::array<std::int32_t, 3>, std::int32_t>> externalWrites_;
};

} // namespace basalt::server::world::terrain::decoration

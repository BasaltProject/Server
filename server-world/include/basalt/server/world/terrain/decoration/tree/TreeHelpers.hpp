#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `Direction.Plane.HORIZONTAL` in declaration order.
struct HorizontalDirection {
    std::int32_t dx = 0;
    std::int32_t dz = 0;

    friend bool operator==(const HorizontalDirection&, const HorizontalDirection&) = default;
};

inline constexpr std::array<HorizontalDirection, 4> kHorizontalDirections{
    HorizontalDirection{.dx = 0, .dz = -1}, HorizontalDirection{.dx = 1, .dz = 0},
    HorizontalDirection{.dx = 0, .dz = 1}, HorizontalDirection{.dx = -1, .dz = 0}};

/// parity: `TreeFeature.validTreePos`.
[[nodiscard]] bool isValidTreePos(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `TrunkPlacer.placeBelowTrunkBlock` with the default dirt provider.
void placeBelowTrunkBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `TreeFeature.validTreePos` plus the persistent-leaf guard.
[[nodiscard]] bool canPlaceLeaf(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

bool tryPlaceLeaf(Chunk& chunk, std::int32_t foliageStateId, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::int32_t decorationHeightAt(const Chunk& chunk, std::int32_t worldX, std::int32_t worldZ,
                                              HeightmapType type);

} // namespace basalt::server::world::terrain::decoration::tree

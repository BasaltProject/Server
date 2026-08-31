#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `Direction.Plane.HORIZONTAL` in declaration order, which is what `random.nextInt(4)`
/// indexes into. Reordering this rotates every tree that leans.
struct HorizontalDirection {
    std::int32_t dx = 0;
    std::int32_t dz = 0;

    friend bool operator==(const HorizontalDirection&, const HorizontalDirection&) = default;
};

inline constexpr std::array<HorizontalDirection, 4> kHorizontalDirections{
    HorizontalDirection{.dx = 0, .dz = -1}, HorizontalDirection{.dx = 1, .dz = 0},
    HorizontalDirection{.dx = 0, .dz = 1}, HorizontalDirection{.dx = -1, .dz = 0}};

/// parity: `TreeFeature.validTreePos`, read through the decoration view so a tree sees what earlier
/// features in the same step wrote.
[[nodiscard]] bool isValidTreePos(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

/// Writes a tree block, dropping anything outside the world's vertical bounds.
void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `TrunkPlacer.placeBelowTrunkBlock` with the default dirt provider, which leaves the soil
/// a tree may already be standing on alone.
void placeBelowTrunkBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `TreeFeature.validTreePos` plus the persistent-leaf guard: a player-placed leaf is never
/// overwritten by generation.
[[nodiscard]] bool canPlaceLeaf(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z);

/// Places a leaf, waterlogging it when it lands in source water.
bool tryPlaceLeaf(Chunk& chunk, std::int32_t foliageStateId, std::int32_t x, std::int32_t y, std::int32_t z);

/// The chunk's own heightmap inside it, and a scan of what decoration wrote outside it.
[[nodiscard]] std::int32_t decorationHeightAt(const Chunk& chunk, std::int32_t worldX, std::int32_t worldZ,
                                              HeightmapType type);

} // namespace basalt::server::world::terrain::decoration::tree

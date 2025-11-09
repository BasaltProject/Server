#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// The buried treasure: one chest, sunk until it finds stone, with a shell of soft blocks around it.
/// parity: `BuriedTreasureStructure`. The piece is one block at offset 9 in both axes, so it never
/// leaves its chunk; the walk down is what buries a chest that lands over a sea cave.
class BuriedTreasureStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "buried_treasure";
    /// First of the five structures of `underground_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 0;
    static constexpr std::int32_t kUndergroundStructuresStep = 3;

    BuriedTreasureStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> beachBiomes,
                                     SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    /// parity: the six-direction shell. A neighbour that is air or liquid gets the floor block when
    /// what is under it is also open (so the chest is not left over a hole), and the soft block
    /// otherwise. Up never gets the floor treatment, which is the jar's own exception.
    static void shellAround(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t floorStateId,
                            std::int32_t softStateId);

    /// parity: `StructurePiece.createChest` with an explicit position, plus `reorient`.
    static void placeChest(Chunk& chunk, std::int64_t seed, std::int32_t x, std::int32_t y, std::int32_t z);

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> beachBiomes_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

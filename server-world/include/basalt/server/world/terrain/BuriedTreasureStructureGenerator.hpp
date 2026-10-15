#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// parity: `BuriedTreasureStructure`.
class BuriedTreasureStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "buried_treasure";
    static constexpr std::int32_t kStructureIndex = 0;
    static constexpr std::int32_t kUndergroundStructuresStep = 3;

    BuriedTreasureStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> beachBiomes,
                                     SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    void bury(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool forPlacement = false);

    /// parity: the six-direction shell. A neighbour that is air or liquid gets the floor block when.
    static void shellAround(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t floorStateId,
                            std::int32_t softStateId);

    /// parity: `StructurePiece.createChest` with an explicit position, plus `reorient`.
    static void placeChest(Chunk& chunk, std::int64_t seed, std::int32_t x, std::int32_t y, std::int32_t z);

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> beachBiomes_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain {

/// heightmaps. parity: `ShipwreckStructure`. The height is the mean of the heightmap over the whole
class ShipwreckStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using HeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "shipwreck";
    static constexpr std::int32_t kSunkStructureIndex = 35;
    static constexpr std::int32_t kBeachedStructureIndex = 36;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    ShipwreckStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> oceanBiomes,
                                std::set<std::string, std::less<>> beachBiomes, TemplateOf templateOf,
                                HeightAt surfaceHeightAt, HeightAt oceanFloorAt, WorldHeightAt worldHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ, bool beached) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool beached) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool beached) const;

private:
    struct HullStart {
        const structure::StructureTemplate* piece = nullptr;
        structure::StructureRotation rotation = structure::StructureRotation::None;
        structure::StructurePos settled{};
    };

    [[nodiscard]] std::optional<HullStart> hullStart(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                                     bool beached) const;

    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: the weighted draw in `ChunkGenerator`, both weights one, `shipwreck` first.
    [[nodiscard]] std::optional<bool> chooseVariant(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: `ShipwreckPiece.postProcess`. The sunk hull takes the mean ocean floor of its.
    [[nodiscard]] std::optional<structure::StructurePos> settle(const structure::StructureTemplate& piece,
                                                                structure::StructurePos origin, bool beached,
                                                                random::RandomSource& random) const;

    /// parity: `ShipwreckPiece.handleDataMarker`.
    static void placeMarkerChests(Chunk& chunk, const structure::StructureTemplate& piece,
                                  structure::StructurePos position, structure::StructureRotation rotation,
                                  const structure::StructureBoundingBox& clip, random::RandomSource& random);

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> oceanBiomes_;
    std::set<std::string, std::less<>> beachBiomes_;
    TemplateOf templateOf_;
    HeightAt surfaceHeightAt_;
    HeightAt oceanFloorAt_;
    WorldHeightAt worldHeightAt_;
};

} // namespace basalt::server::world::terrain

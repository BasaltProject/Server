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

/// The two shipwrecks, sunk and beached: the same structure with different template lists and
/// heightmaps. parity: `ShipwreckStructure`. The height is the mean of the heightmap over the whole
/// footprint, not the column it started from, so a wreck across a slope half buries itself.
class ShipwreckStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using HeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "shipwreck";
    /// Eighteenth and nineteenth of the 26 of `surface_structures`, in registry order.
    static constexpr std::int32_t kSunkStructureIndex = 17;
    static constexpr std::int32_t kBeachedStructureIndex = 18;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    ShipwreckStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> oceanBiomes,
                                std::set<std::string, std::less<>> beachBiomes, TemplateOf templateOf,
                                HeightAt surfaceHeightAt, HeightAt oceanFloorAt, WorldHeightAt worldHeightAt);

    /// A hull is up to 32 blocks long, so it spills into its neighbours. Every chunk it touches
    /// places its own part, which is what vanilla does by giving each chunk the same start and
    /// clipping the write; here that means walking the nearby cells rather than only this chunk's.
    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Which of the two the chunk gets, or nothing when neither fits its biome.
    /// parity: the weighted draw in `ChunkGenerator`, both weights one, `shipwreck` first.
    [[nodiscard]] std::optional<bool> chooseVariant(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: `ShipwreckPiece.postProcess`. The sunk hull takes the mean ocean floor of its
    /// footprint; the beached one takes the lowest surface, then sinks half its own height and one
    /// to three blocks more.
    [[nodiscard]] std::optional<structure::StructurePos> settle(const structure::StructureTemplate& piece,
                                                                structure::StructurePos origin, bool beached,
                                                                random::RandomSource& random) const;

    /// parity: `ShipwreckPiece.handleDataMarker`, which turns the three chest markers into a loot
    /// table on the container below the marker.
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

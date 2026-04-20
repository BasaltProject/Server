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
#include <vector>

namespace basalt::server::world::terrain {

/// The warm and cold ocean ruins, which share a structure set and differ in their templates.
/// parity: `OceanRuinStructure` plus `OceanRuinPieces`. A cold ruin is three templates stacked at one
/// place, each rotted harder than the last, and a large ruin drags a cluster of small ones around it.
class OceanRuinStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using OceanFloorAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "ocean_ruin";
    /// Position among the 44 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kColdStructureIndex = 25;
    static constexpr std::int32_t kWarmStructureIndex = 26;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    OceanRuinStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> coldBiomes,
                                std::set<std::string, std::less<>> warmBiomes, TemplateOf templateOf,
                                OceanFloorAt oceanFloorAt, WorldHeightAt worldHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`: where a ruin of this kind (`cold` picks the templates
    /// and loot) started at this chunk would write. Ruins never fail to generate, so the bounds are
    /// always the conservative reach of a parent ruin plus its cluster: two chunks out in every
    /// direction.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ, bool cold) const;

    /// Writes the part of the ruin of this kind started at (chunkX, chunkZ) that lands in `chunk`,
    /// like the worldgen pass does, but without the grid and biome checks and without recording a start.
    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool cold) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    /// Whether a ruin of that kind starts in this chunk, which is what `/locate` and an explorer map
    /// ask.
    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool warm) const;

private:
    /// `forPlacementCold`, when set, means `/place structure`: the grid and biome checks are skipped
    /// and the variant is the one named on the command line.
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   std::optional<bool> forPlacementCold = std::nullopt) const;

    /// parity: `addPiece`. A warm ruin is one template; a cold one is three, rotted 0.9, 0.7, 0.5.
    [[nodiscard]] std::vector<structure::StructureBoundingBox>
    addPiece(Chunk& chunk, std::int64_t seed, const structure::StructureBoundingBox& clip, random::RandomSource& random,
             random::RandomSource& pick, structure::StructurePos position, structure::StructureRotation rotation,
             bool cold, bool large, float baseIntegrity) const;

    /// parity: `addClusterRuins`. Eight candidate spots are drawn around the parent, four to eight of
    /// them are taken at random, and one is dropped if its box would touch the parent's.
    [[nodiscard]] std::vector<structure::StructureBoundingBox>
    addCluster(Chunk& chunk, std::int64_t seed, const structure::StructureBoundingBox& clip,
               random::RandomSource& random, random::RandomSource& pick, structure::StructurePos parent,
               structure::StructureRotation parentRotation, bool cold) const;

    /// parity: `OceanRuinPiece.postProcess` plus its `getHeight`. The piece drops to the ocean floor
    /// and then, if its footprint is mostly a hole, sinks to the bottom of that hole instead of
    /// bridging it.
    [[nodiscard]] structure::StructurePos settle(const Chunk& chunk, const structure::StructureTemplate& piece,
                                                 structure::StructurePos position,
                                                 structure::StructureRotation rotation) const;

    [[nodiscard]] std::optional<bool> chooseVariant(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> coldBiomes_;
    std::set<std::string, std::less<>> warmBiomes_;
    TemplateOf templateOf_;
    OceanFloorAt oceanFloorAt_;
    WorldHeightAt worldHeightAt_;
};

} // namespace basalt::server::world::terrain

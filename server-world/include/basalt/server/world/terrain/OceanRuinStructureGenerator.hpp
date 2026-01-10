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
    /// Eighth and ninth of the 26 of `surface_structures`: cold first, then warm.
    static constexpr std::int32_t kColdStructureIndex = 7;
    static constexpr std::int32_t kWarmStructureIndex = 8;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    OceanRuinStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> coldBiomes,
                                std::set<std::string, std::less<>> warmBiomes, TemplateOf templateOf,
                                OceanFloorAt oceanFloorAt, WorldHeightAt worldHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

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

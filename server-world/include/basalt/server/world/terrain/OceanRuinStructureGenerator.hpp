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

/// parity: `OceanRuinStructure` plus `OceanRuinPieces`.
class OceanRuinStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using OceanFloorAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "ocean_ruin";
    static constexpr std::int32_t kColdStructureIndex = 25;
    static constexpr std::int32_t kWarmStructureIndex = 26;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    OceanRuinStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> coldBiomes,
                                std::set<std::string, std::less<>> warmBiomes, TemplateOf templateOf,
                                OceanFloorAt oceanFloorAt, WorldHeightAt worldHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ, bool cold) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool cold) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool warm) const;

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   std::optional<bool> forPlacementCold = std::nullopt) const;

    /// parity: `addPiece`.
    [[nodiscard]] std::vector<structure::StructureBoundingBox>
    addPiece(Chunk& chunk, std::int64_t seed, const structure::StructureBoundingBox& clip, random::RandomSource& random,
             random::RandomSource& pick, structure::StructurePos position, structure::StructureRotation rotation,
             bool cold, bool large, float baseIntegrity) const;

    /// parity: `addClusterRuins`.
    [[nodiscard]] std::vector<structure::StructureBoundingBox>
    addCluster(Chunk& chunk, std::int64_t seed, const structure::StructureBoundingBox& clip,
               random::RandomSource& random, random::RandomSource& pick, structure::StructurePos parent,
               structure::StructureRotation parentRotation, bool cold) const;

    /// parity: `OceanRuinPiece.postProcess` plus its `getHeight`.
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

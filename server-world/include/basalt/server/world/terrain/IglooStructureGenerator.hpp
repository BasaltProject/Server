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

/// The igloo, and half the time the ladder shaft and laboratory under it. parity: `IglooStructure`
class IglooStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "igloo";
    static constexpr std::int32_t kStructureIndex = 21;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    IglooStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> iglooBiomes, TemplateOf templateOf,
                            SurfaceHeightAt surfaceHeightAt, WorldHeightAt worldHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    /// parity: `IglooStructure.findGenerationPoint`'s draw order.
    void placeIgloo(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                    bool forPlacement = false);

    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    place(Chunk& chunk, const structure::StructureBoundingBox& clip, random::RandomSource& random,
          std::string_view name, structure::StructurePos start, structure::StructureRotation rotation,
          std::int32_t depth) const;

    /// parity: `handleDataMarker`.
    static void placeChestMarker(Chunk& chunk, const structure::StructureTemplate& piece,
                                 structure::StructurePos position, structure::StructureRotation rotation,
                                 structure::StructurePos pivot, const structure::StructureBoundingBox& clip,
                                 random::RandomSource& random);

    /// parity: the trapdoor square is filled with snow when there is no ladder under it, which is.
    static void closeTrapDoorWithoutShaft(Chunk& chunk, structure::StructurePos position,
                                          structure::StructureRotation rotation, structure::StructurePos pivot,
                                          const structure::StructureBoundingBox& clip);

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> iglooBiomes_;
    TemplateOf templateOf_;
    SurfaceHeightAt surfaceHeightAt_;
    WorldHeightAt worldHeightAt_;
};

} // namespace basalt::server::world::terrain

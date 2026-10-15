#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MansionPlacer.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain {

/// The woodland mansion. parity: `WoodlandMansionStructure`. Ninety blocks across at its widest, so
class WoodlandMansionStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "mansion";
    static constexpr std::int32_t kStructureIndex = 23;

    WoodlandMansionStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> mansionBiomes,
                                      TemplateOf templateOf, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ);

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   bool forPlacement = false);

    [[nodiscard]] std::vector<structure::MansionPiece> mansionStart(std::int64_t seed, std::int32_t chunkX,
                                                                    std::int32_t chunkZ, bool forPlacement,
                                                                    structure::StructureRotation& rotation) const;

    /// parity: the `Chest*` and mob markers.
    void placeMarkers(Chunk& chunk, const structure::StructureTemplate& piece,
                      const structure::MansionPiece& mansionPiece, const structure::StructureBoundingBox& clip,
                      random::RandomSource& random) const;

    /// parity: `getLowestYIn5by5BoxOffset7Blocks`.
    [[nodiscard]] std::optional<structure::StructurePos> lowestCornerOf(std::int32_t chunkX, std::int32_t chunkZ,
                                                                        structure::StructureRotation rotation) const;

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> mansionBiomes_;
    TemplateOf templateOf_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

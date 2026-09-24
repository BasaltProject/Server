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

namespace basalt::server::world::terrain {

/// The woodland mansion. parity: `WoodlandMansionStructure`. Ninety blocks across at its widest, so
/// it reaches six chunks out from the one it starts in and every chunk it touches lays its own part.
class WoodlandMansionStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "mansion";
    /// Position among the 44 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 23;

    WoodlandMansionStructureGenerator(BiomeAt biomeAt, std::set<std::string, std::less<>> mansionBiomes,
                                      TemplateOf templateOf, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    /// Whether a mansion starts in this chunk, which is what `/locate` asks.
    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: the `Chest*` and mob markers. The chest faces the way its marker names once the
    /// house's own rotation is applied; a mob marker leaves air behind it.
    void placeMarkers(Chunk& chunk, const structure::StructureTemplate& piece,
                      const structure::MansionPiece& mansionPiece, const structure::StructureBoundingBox& clip,
                      random::RandomSource& random) const;

    /// parity: `getLowestYIn5by5BoxOffset7Blocks`. The corner box is measured from the seventh block
    /// of the chunk and leans the way the house is turned, and a mansion below y 60 is refused.
    [[nodiscard]] std::optional<structure::StructurePos> lowestCornerOf(std::int32_t chunkX, std::int32_t chunkZ,
                                                                        structure::StructureRotation rotation) const;

    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> mansionBiomes_;
    TemplateOf templateOf_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

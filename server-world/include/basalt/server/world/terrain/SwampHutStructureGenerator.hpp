#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::world::terrain {

/// The swamp hut, vanilla's one-piece scattered structure. parity: `SwampHutStructure`. The piece is
/// 7 by 9 from the chunk's own minimum corner, so it never leaves its chunk whichever way it faces.
class SwampHutStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    /// The noise column's own surface, not the chunk heightmap: the carvers have already run.
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "swamp_hut";
    /// Position among the 44 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 38;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    SwampHutStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`: the piece never leaves its start chunk, so the bounds
    /// are that whole chunk. The grid and biome checks are skipped, as the command skips them.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    /// Writes the hut started at (chunkX, chunkZ) into `chunk`, like the worldgen pass does, but
    /// without the grid and biome checks and without recording a start.
    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    /// Whether a hut starts in this chunk, which is what `/locate` asks.
    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

private:
    /// The hut itself: orientation draw, settle and write. `forPlacement` skips the start record,
    /// which `PlaceCommand.placeStructure` does not register.
    void placeHut(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, bool forPlacement = false);

    BiomeAt biomeAt_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

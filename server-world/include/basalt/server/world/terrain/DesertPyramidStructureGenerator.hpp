#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DesertPyramidPiece.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::world::terrain {

/// The desert pyramid. parity: `DesertPyramidStructure`, one piece 21 blocks square, so every chunk
/// it reaches into places its own part of it.
class DesertPyramidStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "desert_pyramid";
    /// Position among the 44 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 19;

    DesertPyramidStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`: the settled bounds of the pyramid started at this
    /// chunk with the grid and biome checks skipped, or nothing when its own conditions (the sea
    /// level floor) reject the chunk.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    /// Writes the part of the pyramid started at (chunkX, chunkZ) that lands in `chunk`, like the
    /// worldgen pass does, but without the grid and biome checks and without recording a start.
    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    /// Whether a pyramid starts in this chunk, which is what `/locate` asks.
    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
        return startPiece(seed, chunkX, chunkZ).has_value();
    }

private:
    /// The settled piece of a pyramid starting in this chunk, or nothing. `forPlacement` skips the
    /// grid and biome checks, which is what `/place structure` does.
    [[nodiscard]] std::optional<DesertPyramidPiece> startPiece(std::int64_t seed, std::int32_t chunkX,
                                                               std::int32_t chunkZ, bool forPlacement = false) const;

    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   bool forPlacement = false) const;

    /// parity: `-random.nextInt(3)` off the placing chunk's own feature random, so in the jar the
    /// height depends on which chunk reached the pyramid first. Basalt draws it from the chunk the
    /// pyramid starts in instead: every chunk then settles it at the same y, which is the point.
    [[nodiscard]] static std::int32_t groundOffset(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: `DesertPyramidStructure.afterPlace`. Five to seven of the candidates become
    /// suspicious sand and the rest plain sand, drawn from a shuffle seeded off the world seed at
    /// the piece's centre, so every chunk that runs this agrees on which ones they are.
    static void placeSuspiciousSand(Chunk& chunk, const structure::StructureBoundingBox& clip, std::int64_t seed,
                                    const DesertPyramidPiece& piece);

    static void setSuspiciousSand(Chunk& chunk, const structure::StructureBoundingBox& clip,
                                  structure::StructurePos position);

    BiomeAt biomeAt_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

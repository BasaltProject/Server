#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DesertPyramidPiece.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::world::terrain {

/// The desert pyramid. parity: `DesertPyramidStructure`, one piece 21 blocks square.
class DesertPyramidStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    static constexpr std::string_view kName = "desert_pyramid";
    static constexpr std::int32_t kStructureIndex = 19;

    DesertPyramidStructureGenerator(BiomeAt biomeAt, SurfaceHeightAt surfaceHeightAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

    [[nodiscard]] bool startsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const {
        return startPiece(seed, chunkX, chunkZ).has_value();
    }

private:
    [[nodiscard]] std::optional<DesertPyramidPiece> startPiece(std::int64_t seed, std::int32_t chunkX,
                                                               std::int32_t chunkZ, bool forPlacement = false) const;

    void placeFrom(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                   bool forPlacement = false) const;

    /// parity: `-random.nextInt(3)` off the placing chunk's own feature random.
    [[nodiscard]] static std::int32_t groundOffset(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: `DesertPyramidStructure.afterPlace`. Five to seven of the candidates become.
    static void placeSuspiciousSand(Chunk& chunk, const structure::StructureBoundingBox& clip, std::int64_t seed,
                                    const DesertPyramidPiece& piece);

    static void setSuspiciousSand(Chunk& chunk, const structure::StructureBoundingBox& clip,
                                  structure::StructurePos position);

    BiomeAt biomeAt_;
    SurfaceHeightAt surfaceHeightAt_;
};

} // namespace basalt::server::world::terrain

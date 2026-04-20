#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/EndCityPieces.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// Places exact procedural End City starts and their template pieces before End decoration.
class EndCityStructureGenerator {
public:
    using SurfaceHeightOverride = std::function<std::int32_t(std::int32_t, std::int32_t)>;

    EndCityStructureGenerator(NoiseRouter router, const TheEndBiomeSource& biomeSource,
                              structure::TemplateLoader templateLoader,
                              SurfaceHeightOverride surfaceHeightOverride = nullptr);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`: the bounds of the city started at this chunk with the
    /// grid and biome checks skipped, or nothing when its own conditions (the y 60 floor) reject the
    /// chunk or the assembly came up empty.
    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ);

    /// Writes the part of the city started at (chunkX, chunkZ) that lands in `chunk`, like the
    /// worldgen pass does, but without the grid and biome checks and without recording a start.
    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// The city that starts in a candidate chunk, assembled once and kept.
    [[nodiscard]] const std::vector<endCityPieces::Piece>& cityAt(std::int64_t seed, std::int32_t candidateChunkX,
                                                                  std::int32_t candidateChunkZ);

    [[nodiscard]] static const structure::StructurePlacementGrid& grid();

private:
    struct CityCacheKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const CityCacheKey&, const CityCacheKey&) = default;
    };

    struct CityReference {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        const std::vector<endCityPieces::Piece>* pieces = nullptr;
    };

    [[nodiscard]] std::vector<CityReference> referencedCities(std::int32_t chunkX, std::int32_t chunkZ,
                                                              std::int64_t seed,
                                                              const structure::StructureBoundingBox& clip);

    void handleMarkers(Chunk& chunk, const endCityPieces::Piece& piece, const structure::StructureBoundingBox& clip,
                       random::RandomSource& random) const;

    static void placeElytraFrame(Chunk& chunk, structure::StructurePos position, structure::StructureRotation rotation);

    [[nodiscard]] std::int32_t surfaceHeightAt(std::int32_t x, std::int32_t z);

    [[nodiscard]] DensityChunkSampler& samplerAt(std::int32_t x, std::int32_t z);

    /// The city a start at this chunk assembles into. `forPlacement` skips the biome check and is
    /// never cached, so it can differ from what worldgen holds for the same chunk.
    [[nodiscard]] std::vector<endCityPieces::Piece> buildCity(std::int64_t seed, std::int32_t candidateChunkX,
                                                              std::int32_t candidateChunkZ, bool forPlacement);

    NoiseRouter router_;
    const TheEndBiomeSource* biomeSource_;
    structure::TemplateLoader templateLoader_;
    SurfaceHeightOverride surfaceHeightOverride_;
    std::mutex mutex_;
    std::map<std::pair<std::int32_t, std::int32_t>, std::unique_ptr<DensityChunkSampler>> samplers_;
    std::map<CityCacheKey, std::vector<endCityPieces::Piece>> cities_;
};

} // namespace basalt::server::world::terrain

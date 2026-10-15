#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/JigsawAssembly.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>
#include <basalt/server/world/terrain/NetherFortressPieces.hpp>
#include <basalt/server/world/terrain/NetherRuinedPortalGenerator.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain {

enum class NetherPlacedStructure : std::uint8_t { Fortress, Bastion, Fossil };

class NetherStructureGenerator {
public:
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    NetherStructureGenerator(NoiseRouter router, const NetherBiomeSource& biomeSource,
                             structure::TemplateLoader templateLoader, PoolLookup poolLookup = nullptr);

    void generateInto(Chunk& chunk, std::int64_t seed);

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, NetherPlacedStructure structure);

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                 NetherPlacedStructure structure);

    /// parity: `PlaceCommand.placeStructure` for `ruined_portal_nether`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    netherPortalBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    void netherPortalPlaceAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: `Beardifier.forStructuresInChunk`, the sampler the chunk's context binds.
    [[nodiscard]] densityfunction::Sampler beardifierForChunk(std::int32_t chunkX, std::int32_t chunkZ,
                                                              std::int64_t seed);

private:
    struct FossilStart {
        structure::StructurePos position;
        structure::StructureRotation rotation = structure::StructureRotation::None;
        std::string templateName;
    };

    struct FossilReference {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        FossilStart start;
        const structure::StructureTemplate* piece = nullptr;
        structure::StructureBoundingBox boundingBox;
    };

    struct CacheKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const CacheKey&, const CacheKey&) = default;
    };

    void placeBastions(Chunk& chunk, std::int64_t seed);
    void placeFortresses(Chunk& chunk, std::int64_t seed);
    void placeNetherFossils(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] bool selectsBastion(std::int64_t seed, std::int32_t candidateChunkX,
                                      std::int32_t candidateChunkZ) const;
    [[nodiscard]] bool selectsFortress(std::int64_t seed, std::int32_t candidateChunkX,
                                       std::int32_t candidateChunkZ) const;

    [[nodiscard]] const std::vector<structure::PlacedStructurePiece>&
    assembledBastion(std::int64_t seed, std::int32_t candidateChunkX, std::int32_t candidateChunkZ);

    [[nodiscard]] const std::vector<netherFortressPieces::Piece>&
    assembledFortress(std::int64_t seed, std::int32_t candidateChunkX, std::int32_t candidateChunkZ);

    [[nodiscard]] std::vector<FossilReference> referencedNetherFossils(std::int32_t chunkX, std::int32_t chunkZ,
                                                                       std::int64_t seed);

    [[nodiscard]] std::optional<FossilStart> netherFossilStart(std::int32_t candidateChunkX,
                                                               std::int32_t candidateChunkZ, std::int64_t seed,
                                                               bool forPlacement = false);

    [[nodiscard]] std::vector<netherFortressPieces::Piece> fortressStart(std::int64_t seed, std::int32_t chunkX,
                                                                         std::int32_t chunkZ) const;
    [[nodiscard]] std::vector<structure::PlacedStructurePiece> bastionStart(std::int64_t seed, std::int32_t chunkX,
                                                                            std::int32_t chunkZ) const;
    [[nodiscard]] const structure::StructureTemplate* templateOf(std::string_view name) const;

    /// parity: `RuinedPortalPiece` reads the level.
    [[nodiscard]] std::int32_t baseStateAt(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] DensityChunkSampler& samplerAt(std::int32_t x, std::int32_t z);

    void placeDriedGhast(Chunk& chunk, std::int64_t seed, const structure::StructureBoundingBox& fossilBox,
                         const structure::StructureBoundingBox& clip) const;

    NoiseRouter router_;
    const NetherBiomeSource* biomeSource_;
    structure::TemplateLoader templateLoader_;
    PoolLookup poolLookup_;
    std::unique_ptr<NetherRuinedPortalGenerator> ruinedPortals_;
    std::mutex mutex_;
    std::map<std::pair<std::int32_t, std::int32_t>, std::unique_ptr<DensityChunkSampler>> samplers_;
    std::map<CacheKey, std::vector<structure::PlacedStructurePiece>> bastions_;
    std::map<CacheKey, std::vector<netherFortressPieces::Piece>> fortresses_;
};

} // namespace basalt::server::world::terrain

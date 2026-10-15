#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/JigsawAssembly.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

struct LocatedVillage {
    wire::common::Identifier structure;
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
};

class VillageStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    static constexpr std::int32_t kDefaultSearchCells = 100;

    struct Variant {
        wire::common::Identifier id;
        std::string startPool;
        std::set<std::string, std::less<>> biomes;
        std::int32_t structureIndex = 0;
    };

    VillageStructureGenerator(NoiseRouter router, structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                              std::int32_t maxDepth, std::int32_t maxDistanceFromCenter, PoolLookup poolLookup,
                              structure::PoolTemplateLoader templateLoader);

    VillageStructureGenerator(NoiseRouter router, structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                              std::int32_t maxDepth, std::int32_t maxDistanceFromCenter, PoolLookup poolLookup,
                              structure::PoolTemplateLoader templateLoader, std::vector<Variant> variants);

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] std::pair<std::size_t, std::int64_t> assemblyStats() const;

    void generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    [[nodiscard]] static const std::vector<wire::common::Identifier>& structureIds();

    /// parity: `ChunkGenerator.findNearestMapStructure`.
    [[nodiscard]] std::optional<LocatedVillage>
    locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ,
           const wire::common::Identifier* structure, std::int32_t maxCells = kDefaultSearchCells,
           const std::function<bool(std::int32_t, std::int32_t)>& skip = {}) const;

    [[nodiscard]] std::optional<std::vector<structure::PlacedStructurePiece>>
    assembleAt(const wire::common::Identifier& structure, std::int32_t chunkX, std::int32_t chunkZ,
               std::int64_t seed) const;

    void placeAt(Chunk& chunk, const wire::common::Identifier& structure, std::int32_t chunkX, std::int32_t chunkZ,
                 std::int64_t seed) const;

    /// parity: `PlaceCommand.placeJigsaw`.
    [[nodiscard]] std::vector<structure::PlacedStructurePiece> assemblePool(std::string_view pool, std::int32_t chunkX,
                                                                            std::int32_t chunkZ, std::int32_t maxDepth,
                                                                            std::int64_t seed) const;

    [[nodiscard]] bool hasPool(std::string_view pool) const { return poolLookup_(pool) != nullptr; }

    void placeInto(Chunk& chunk, const std::vector<structure::PlacedStructurePiece>& pieces, std::int64_t seed) const;

    /// parity: `FlatLevelSource.getBaseHeight`.
    void useFlatSurface(std::int32_t height) noexcept { flatSurface_ = height; }

    /// parity: `Beardifier.forStructuresInChunk`, this structure's share of it.
    void addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

private:
    struct CandidateKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const CandidateKey&, const CandidateKey&) = default;
    };

    struct VillageStart {
        const Variant* variant = nullptr;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        std::vector<structure::PlacedStructurePiece> pieces;
    };

    using Samplers = std::map<std::int64_t, DensityChunkSampler>;

    [[nodiscard]] std::vector<std::shared_ptr<const VillageStart>>
    referencedStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                     const structure::StructureBoundingBox& clip);

    [[nodiscard]] std::shared_ptr<const VillageStart> assembledStart(std::int32_t candidateChunkX,
                                                                     std::int32_t candidateChunkZ, std::int64_t seed);

    /// parity: `villages.json`'s five entries in file order.
    [[nodiscard]] static const std::vector<Variant>& villageVariants();

    /// parity: `ChunkGenerator`'s structure-set loop for multi-entry sets.
    [[nodiscard]] const Variant* selectVariant(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                               std::int64_t seed, Samplers& samplers) const;

    [[nodiscard]] std::int32_t surfaceHeightAt(std::int32_t x, std::int32_t z, Samplers& samplers) const;

    [[nodiscard]] std::vector<structure::PlacedStructurePiece> assemble(std::string_view startPool, std::int32_t chunkX,
                                                                        std::int32_t chunkZ, std::int32_t maxDepth,
                                                                        std::int64_t seed, Samplers& samplers,
                                                                        random::RandomSource& random) const;

    NoiseRouter router_;
    std::optional<std::int32_t> flatSurface_;
    structure::StructurePlacementGrid grid_;
    BiomeAt biomeAt_;
    std::int32_t maxDepth_;
    std::int32_t maxDistanceFromCenter_;
    PoolLookup poolLookup_;
    structure::PoolTemplateLoader templateLoader_;
    std::vector<Variant> variants_;
    std::vector<const Variant*> placementVariants_;
    mutable std::mutex mutex_;
    std::map<CandidateKey, std::shared_ptr<const VillageStart>> cache_;
};

} // namespace basalt::server::world::terrain

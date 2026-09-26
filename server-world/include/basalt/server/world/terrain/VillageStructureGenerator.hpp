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

/// Where a village was found, and which of the five it is.
struct LocatedVillage {
    wire::common::Identifier structure;
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
};

/// Places villages during chunk generation, picking the variant from the biome at each candidate.
///
/// parity, narrowed: vanilla caches the assembled pieces in the placement chunk; chunks here are
/// stateless, so `generateInto` reassembles each nearby candidate. Starts use the legacy structure seed.
class VillageStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    /// Vanilla searches 100 cells for a village, which at spacing 34 is a long way out.
    static constexpr std::int32_t kDefaultSearchCells = 100;

    /// One entry of a multi-structure set: its structure, start pool, biomes without namespace and
    /// index within `surface_structures`.
    struct Variant {
        wire::common::Identifier id;
        std::string startPool;
        std::set<std::string, std::less<>> biomes;
        std::int32_t structureIndex = 0;
    };

    VillageStructureGenerator(NoiseRouter router, structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                              std::int32_t maxDepth, std::int32_t maxDistanceFromCenter, PoolLookup poolLookup,
                              structure::PoolTemplateLoader templateLoader);

    /// The same generator over another set of equally weighted jigsaw structures, such as the 26.3
    /// `abandoned_camp` set. `variants` is in the set's file order, which is the order the draw walks.
    VillageStructureGenerator(NoiseRouter router, structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                              std::int32_t maxDepth, std::int32_t maxDistanceFromCenter, PoolLookup poolLookup,
                              structure::PoolTemplateLoader templateLoader, std::vector<Variant> variants);

    /// The structures this generator makes.
    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    /// Cached candidate count and their total placed pieces, for the timings memory breakdown.
    [[nodiscard]] std::pair<std::size_t, std::int64_t> assemblyStats() const;

    void generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    /// Every village structure this generator can make, by the id `/locate` and `/place` name.
    [[nodiscard]] static const std::vector<wire::common::Identifier>& structureIds();

    /// The nearest chunk where `structure` would generate, searched ring by ring over the placement grid.
    ///
    /// parity: `ChunkGenerator.findNearestMapStructure`: each candidate runs generation's weighted draw
    /// against its centre biome, so one that draws a different village is not a match.
    [[nodiscard]] std::optional<LocatedVillage>
    locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ,
           const wire::common::Identifier* structure, std::int32_t maxCells = kDefaultSearchCells,
           const std::function<bool(std::int32_t, std::int32_t)>& skip = {}) const;

    /// Assembles `structure` at a chunk that is not its candidate, which is what `/place structure`
    /// does: vanilla generates the structure where it was told, skipping the placement grid and the
    /// biome check that decide where one appears on its own.
    [[nodiscard]] std::optional<std::vector<structure::PlacedStructurePiece>>
    assembleAt(const wire::common::Identifier& structure, std::int32_t chunkX, std::int32_t chunkZ,
               std::int64_t seed) const;

    /// Assembles any template pool by name, as `/place jigsaw` does: just a start pool and a depth.
    ///
    /// parity: `PlaceCommand.placeJigsaw`; its `target` is accepted and ignored, since this assembly
    /// grows from every jigsaw of the start piece anyway.
    [[nodiscard]] std::vector<structure::PlacedStructurePiece> assemblePool(std::string_view pool, std::int32_t chunkX,
                                                                            std::int32_t chunkZ, std::int32_t maxDepth,
                                                                            std::int64_t seed) const;

    /// Whether `pool` is a template pool the operator's own jar actually ships.
    [[nodiscard]] bool hasPool(std::string_view pool) const { return poolLookup_(pool) != nullptr; }

    /// Writes the part of `pieces` that lands in `chunk`. The caller repeats it per chunk they cover.
    void placeInto(Chunk& chunk, const std::vector<structure::PlacedStructurePiece>& pieces, std::int64_t seed) const;

    /// parity: `FlatLevelSource.getBaseHeight`, one surface height everywhere instead of the router's.
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
        /// The chunk the village began in, which is what its structure start belongs to.
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        std::vector<structure::PlacedStructurePiece> pieces;
    };

    /// One sampler per chunk column, alive exactly as long as the assembly that needs it. The plains
    /// pools have 36 terrain-matching entries, each asking once per attempted connection, so reuse
    /// within an assembly is what makes it finish; across assemblies there is nothing to reuse,
    /// because a finished assembly is cached and never recomputed.
    using Samplers = std::map<std::int64_t, DensityChunkSampler>;

    [[nodiscard]] std::vector<std::shared_ptr<const VillageStart>>
    referencedStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                     const structure::StructureBoundingBox& clip);

    /// Shared rather than borrowed: the cache is cleared when it fills, and a pointer into it would
    /// dangle as soon as another worker triggered that mid-read.
    [[nodiscard]] std::shared_ptr<const VillageStart> assembledStart(std::int32_t candidateChunkX,
                                                                     std::int32_t candidateChunkZ, std::int64_t seed);

    /// parity: `villages.json`'s five entries in file order, all weight one, each with its own
    /// `has_structure/village_*` biome tag.
    [[nodiscard]] static const std::vector<Variant>& villageVariants();

    /// parity: `ChunkGenerator`'s structure-set loop for multi-entry sets: a seeded weighted draw
    /// without replacement until a variant's biome tag matches the candidate's centre column, sampled
    /// at the density-derived surface height. No match means no village there.
    [[nodiscard]] const Variant* selectVariant(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                               std::int64_t seed, Samplers& samplers) const;

    /// Scans a `DensityChunkSampler` rather than calling the density graph without a chunk cache.
    /// The chunk's own heightmap is not an option because the column is often outside it.
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
    /// The variants sorted by their index within the surface step, which is the order they are written in.
    std::vector<const Variant*> placementVariants_;
    mutable std::mutex mutex_;
    std::map<CandidateKey, std::shared_ptr<const VillageStart>> cache_;
};

} // namespace basalt::server::world::terrain

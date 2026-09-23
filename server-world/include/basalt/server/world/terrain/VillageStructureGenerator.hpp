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

/// Places villages during chunk generation, choosing the variant from the biome at each candidate
/// position like vanilla's shared `villages` structure set.
///
/// parity, narrowed: vanilla caches the assembled pieces in the placement chunk's saved data, but
/// chunks here are stateless, so `generateInto` recomputes the assembly per nearby candidate and
/// keeps what lands here. Starts are rebuilt with vanilla's independent legacy structure seed.
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

    /// The nearest chunk where `structure` would generate, searched outward over the placement grid.
    ///
    /// parity: `ChunkGenerator.findNearestMapStructure`, which walks the structure set's own cells
    /// ring by ring rather than scanning chunks, and tests each candidate exactly as generation
    /// would: the same weighted draw against the biome at the candidate's own centre column. A
    /// candidate that draws a different village is not a match, which is why this cannot be answered
    /// from the grid alone.
    [[nodiscard]] std::optional<LocatedVillage> locate(std::int64_t seed, std::int32_t fromChunkX,
                                                       std::int32_t fromChunkZ,
                                                       const wire::common::Identifier* structure,
                                                       std::int32_t maxCells = kDefaultSearchCells) const;

    /// Assembles `structure` at a chunk that is not its candidate, which is what `/place structure`
    /// does: vanilla generates the structure where it was told, skipping the placement grid and the
    /// biome check that decide where one appears on its own.
    [[nodiscard]] std::optional<std::vector<structure::PlacedStructurePiece>>
    assembleAt(const wire::common::Identifier& structure, std::int32_t chunkX, std::int32_t chunkZ,
               std::int64_t seed) const;

    /// Assembles any template pool by name, which is what `/place jigsaw` does: no structure, no
    /// placement grid and no biome, just a start pool and a depth.
    ///
    /// parity: `PlaceCommand.placeJigsaw`, minus its `target` argument. Vanilla uses it to pick which
    /// jigsaw block of the start piece the assembly grows from; this assembly grows from every
    /// jigsaw of the start piece, so naming one narrows nothing and the argument is accepted and
    /// ignored rather than pretended.
    [[nodiscard]] std::vector<structure::PlacedStructurePiece> assemblePool(std::string_view pool, std::int32_t chunkX,
                                                                            std::int32_t chunkZ, std::int32_t maxDepth,
                                                                            std::int64_t seed) const;

    /// Whether `pool` is a template pool the operator's own jar actually ships.
    [[nodiscard]] bool hasPool(std::string_view pool) const { return poolLookup_(pool) != nullptr; }

    /// Writes the part of `pieces` that lands in `chunk`. The caller repeats it per chunk they cover.
    void placeInto(Chunk& chunk, const std::vector<structure::PlacedStructurePiece>& pieces, std::int64_t seed) const;

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

    /// parity: `ChunkGenerator`'s own structure-set generation loop, its real branch for multi-entry
    /// sets: the five real `village_*` structures share one `villages` set, all weight one, and are
    /// tried in a seeded weighted draw without replacement until one's `has_structure/village_*`
    /// biome tag matches the biome at the candidate's own centre column, sampled at the
    /// density-derived surface height. No match anywhere means no village at that candidate.
    /// parity: `villages.json`'s five entries in file order, all weight one, each with its own real
    /// `has_structure/village_*` biome tag contents.
    [[nodiscard]] static const std::vector<Variant>& villageVariants();

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

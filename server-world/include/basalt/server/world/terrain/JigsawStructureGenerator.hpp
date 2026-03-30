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

/// Where a start sits and what the assembly calls its heightmap. parity: the `start_height` of a
/// `JigsawStructure`, plus whether the whole thing is projected onto the surface afterwards.
enum class JigsawHeightSource : std::uint8_t {
    /// The real terrain, walked down from the top of the world.
    Surface,
    /// One y for the start and for every child that asks, drawing nothing.
    Fixed,
    /// A uniform y in a range, which costs one draw before the assembly's own.
    Drawn,
};

/// Which chunk a recorded start belongs to, or that this structure records none.
enum class JigsawStartRecords : std::uint8_t { None, FromCandidate, FromFirstPiece };

/// Everything one jigsaw structure differs from another by.
struct JigsawStructureSettings {
    std::string name;
    /// Its index within its generation step, and which step that is.
    std::int32_t structureIndex = 0;
    std::int32_t step = 0;
    std::string startPool;
    /// The jigsaw block the start piece is anchored by, such as the ancient city's `city_anchor`.
    std::string startJigsawName;
    std::int32_t maxDepth = 0;
    std::int32_t maxDistanceFromCenter = 0;

    /// parity: one `pool_aliases` binding. `Random` picks one target for its alias; `RandomGroup`
    /// picks one group and takes every direct binding in it, which is how the trial chamber keeps
    /// `ranged` and `slow_ranged` on the same mob.
    struct PoolAlias {
        enum class Kind : std::uint8_t { Random, RandomGroup };
        Kind kind = Kind::Random;
        std::string alias;
        std::vector<std::string> targets;
        std::vector<std::vector<std::pair<std::string, std::string>>> groups;
    };

    /// Resolved once per start, in this order: the draws they make are part of the world's sequence.
    std::vector<PoolAlias> poolAliases;
    /// parity: `JigsawStructure`'s `dimension_padding`, non-zero only for the trial chamber.
    std::int32_t dimensionPadding = 0;
    bool useExpansionHack = false;
    bool projectStartToHeightmap = false;
    JigsawHeightSource heightSource = JigsawHeightSource::Surface;
    /// The start's own y. For `Drawn` it is the low end of the range.
    std::int32_t startY = 0;
    std::int32_t startYMax = 0;
    TerrainAdjustment adjustment = TerrainAdjustment::BeardThin;
    JigsawStartRecords startRecords = JigsawStartRecords::FromCandidate;
    /// parity: the trail ruins hand their capped processors the world seed to fork positionally.
    bool passWorldSeed = false;
    std::size_t cacheCapacity = 32;
};

/// The four jigsaw structures of the Overworld, which are one generator with different settings:
/// the pillager outpost, the trail ruins, the ancient city and the trial chamber. parity:
/// `JigsawStructure`, whose own differences are exactly the datapack fields above.
class JigsawStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    static constexpr std::int32_t kDefaultSearchCells = 64;

    JigsawStructureGenerator(JigsawStructureSettings settings, NoiseRouter router,
                             structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                             std::set<std::string, std::less<>> biomes, PoolLookup poolLookup,
                             structure::PoolTemplateLoader templateLoader);

    /// The structure this generator makes, by the id `/locate` names.
    [[nodiscard]] wire::common::Identifier id() const { return wire::common::Identifier::vanilla(settings_.name); }

    /// Cached candidate count and their total placed pieces, for the timings memory breakdown.
    [[nodiscard]] std::pair<std::size_t, std::int64_t> assemblyStats() const;

    void generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    /// Adds this chunk's pieces and junctions to the chunk's one beardifier.
    void addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    /// The nearest chunk holding a start, searched outward over the grid's own cells.
    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
    locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ,
           std::int32_t maxCells = kDefaultSearchCells);

private:
    struct CandidateKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const CandidateKey&, const CandidateKey&) = default;
    };

    using Start = std::pair<CandidateKey, const std::vector<structure::PlacedStructurePiece>*>;
    /// One sampler per chunk column, kept for the length of a single candidate's assembly.
    using Samplers = std::map<std::int64_t, DensityChunkSampler>;

    [[nodiscard]] structure::StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ,
                                                            std::int32_t margin) const;

    [[nodiscard]] std::vector<Start> nearbyStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                                  const structure::StructureBoundingBox& clip);

    /// Shared rather than borrowed: the cache is cleared when it fills, and a reference into it
    /// would dangle as soon as another worker triggered that mid-read.
    [[nodiscard]] std::shared_ptr<const std::vector<structure::PlacedStructurePiece>>
    assembledStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ, std::int64_t seed);

    [[nodiscard]] std::int32_t surfaceHeightAt(std::int32_t x, std::int32_t z, Samplers& samplers) const;

    void recordStart(Chunk& chunk, const structure::StructureBoundingBox& clip, CandidateKey origin,
                     const std::vector<structure::PlacedStructurePiece>& pieces) const;

    JigsawStructureSettings settings_;
    NoiseRouter router_;
    structure::StructurePlacementGrid grid_;
    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> biomes_;
    PoolLookup poolLookup_;
    structure::PoolTemplateLoader templateLoader_;
    mutable std::mutex mutex_;
    std::map<CandidateKey, std::shared_ptr<const std::vector<structure::PlacedStructurePiece>>> cache_;
};

/// The four settings the Overworld's jigsaw structures use, and the fixed grids three of them have.
/// The pillager outpost's grid is built by the caller: it carries an exclusion zone naming the
/// village grid, which cannot be stated without it.
namespace jigsawStructures {

[[nodiscard]] const JigsawStructureSettings& pillagerOutpost();
[[nodiscard]] const JigsawStructureSettings& trailRuins();
[[nodiscard]] const JigsawStructureSettings& ancientCity();
[[nodiscard]] const JigsawStructureSettings& trialChamber();

[[nodiscard]] const structure::StructurePlacementGrid& trailRuinsGrid();
[[nodiscard]] const structure::StructurePlacementGrid& ancientCityGrid();
[[nodiscard]] const structure::StructurePlacementGrid& trialChamberGrid();

} // namespace jigsawStructures

} // namespace basalt::server::world::terrain

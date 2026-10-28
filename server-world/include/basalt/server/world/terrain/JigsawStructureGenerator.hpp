#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/JigsawAssembly.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>

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

/// Where a start sits and what the assembly calls its heightmap. parity: jigsaw `start_height`.
enum class JigsawHeightSource : std::uint8_t {
    Surface,
    Fixed,
    Drawn,
};

enum class JigsawStartRecords : std::uint8_t { None, FromCandidate, FromFirstPiece };

struct JigsawStructureSettings {
    std::string name;
    std::int32_t structureIndex = 0;
    std::int32_t step = 0;
    std::string startPool;
    std::string startJigsawName;
    std::int32_t maxDepth = 0;
    std::int32_t maxDistanceFromCenter = 0;

    /// parity: one `pool_aliases` binding.
    struct PoolAlias {
        enum class Kind : std::uint8_t { Random, RandomGroup };
        Kind kind = Kind::Random;
        std::string alias;
        std::vector<std::string> targets;
        std::vector<std::vector<std::pair<std::string, std::string>>> groups;
    };

    std::vector<PoolAlias> poolAliases;
    /// parity: `JigsawStructure`'s `dimension_padding`, non-zero only for the trial chamber.
    std::int32_t dimensionPadding = 0;
    bool useExpansionHack = false;
    bool projectStartToHeightmap = false;
    JigsawHeightSource heightSource = JigsawHeightSource::Surface;
    std::int32_t startY = 0;
    std::int32_t startYMax = 0;
    TerrainAdjustment adjustment = TerrainAdjustment::BeardThin;
    JigsawStartRecords startRecords = JigsawStartRecords::FromCandidate;
    /// parity: the trail ruins hand their capped processors the world seed to fork positionally.
    bool passWorldSeed = false;
    std::size_t cacheCapacity = 32;
};

/// The jigsaw structures: pillager outpost, trail ruins, ancient city and trial chamber.
class JigsawStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    static constexpr std::int32_t kDefaultSearchCells = 64;

    JigsawStructureGenerator(JigsawStructureSettings settings, NoiseRouter router,
                             structure::StructurePlacementGrid grid, BiomeAt biomeAt,
                             std::set<std::string, std::less<>> biomes, PoolLookup poolLookup,
                             structure::PoolTemplateLoader templateLoader);

    [[nodiscard]] const std::string& name() const noexcept { return settings_.name; }

    [[nodiscard]] std::pair<std::size_t, std::int64_t> assemblyStats() const;

    void generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    void addBeard(StructureBeard& beard, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
    locate(std::int64_t seed, std::int32_t fromChunkX, std::int32_t fromChunkZ,
           std::int32_t maxCells = kDefaultSearchCells,
           const std::function<bool(std::int32_t, std::int32_t)>& skip = {});

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<std::vector<structure::PlacedStructurePiece>>
    assembleAt(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    [[nodiscard]] std::optional<structure::StructureBoundingBox> placeBoundsAt(std::int64_t seed, std::int32_t chunkX,
                                                                               std::int32_t chunkZ);

    void placeAt(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

private:
    struct CandidateKey {
        std::int64_t seed = 0;
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;

        friend auto operator<=>(const CandidateKey&, const CandidateKey&) = default;
    };

    using Start = std::pair<CandidateKey, const std::vector<structure::PlacedStructurePiece>*>;
    using Samplers = std::map<std::int64_t, DensityChunkSampler>;

    [[nodiscard]] structure::StructureBoundingBox chunkClip(std::int32_t chunkX, std::int32_t chunkZ,
                                                            std::int32_t margin) const;

    [[nodiscard]] std::vector<Start> nearbyStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                                  const structure::StructureBoundingBox& clip);

    [[nodiscard]] std::shared_ptr<const std::vector<structure::PlacedStructurePiece>>
    assembledStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ, std::int64_t seed);

    [[nodiscard]] std::vector<structure::PlacedStructurePiece>
    buildStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ, std::int64_t seed, bool forPlacement);

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

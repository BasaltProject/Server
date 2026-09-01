#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/structure/JigsawAssembly.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain {

/// The pillager outpost: one jigsaw start from `pillager_outpost/base_plates`, with `beard_thin` over
/// its rigid pieces. parity: `JigsawStructure` over the `pillager_outposts` set. It takes a fifth of
/// its candidate chunks and refuses any within ten of a village, both of which live in the grid.
class PillagerOutpostStructureGenerator {
public:
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using PoolLookup = std::function<const structure::StructureTemplatePool*(std::string_view)>;

    static constexpr std::string_view kName = "pillager_outpost";
    /// Tenth of the 26 structures of `surface_structures`, in registry order.
    static constexpr std::int32_t kStructureIndex = 9;
    static constexpr std::int32_t kSurfaceStructuresStep = 4;

    static constexpr std::int32_t kMaxDepth = 7;
    static constexpr std::int32_t kMaxDistanceFromCenter = 80;
    static constexpr std::string_view kStartPool = "minecraft:pillager_outpost/base_plates";
    static constexpr std::int32_t kDefaultSearchCells = 64;

    PillagerOutpostStructureGenerator(OverworldNoiseRouter router, structure::StructurePlacementGrid grid,
                                      BiomeAt biomeAt, std::set<std::string, std::less<>> outpostBiomes,
                                      PoolLookup poolLookup, structure::PoolTemplateLoader templateLoader);

    /// Cached candidate count and their total placed pieces, for the timings memory breakdown.
    [[nodiscard]] std::pair<std::size_t, std::int64_t> assemblyStats() const;

    void generateInto(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed);

    /// `base` plus this chunk's beard, or `base` unchanged when no outpost reaches it.
    [[nodiscard]] densityfunction::Function beardified(densityfunction::Function base, std::int32_t chunkX,
                                                       std::int32_t chunkZ, std::int64_t seed);

    /// The nearest chunk holding an outpost, searched outward over the grid's own cells.
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

    [[nodiscard]] std::vector<Start> nearbyStarts(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                                  const structure::StructureBoundingBox& clip);

    [[nodiscard]] const std::vector<structure::PlacedStructurePiece>&
    assembledStart(std::int32_t candidateChunkX, std::int32_t candidateChunkZ, std::int64_t seed);

    /// parity, narrowed the same way the village generator narrows it: the jar tests the biome at
    /// the centre of the start piece's box, which is only known once the piece is placed. The
    /// candidate chunk's own centre column stands in for it.
    [[nodiscard]] bool inOutpostBiome(std::int32_t candidateChunkX, std::int32_t candidateChunkZ,
                                      Samplers& samplers) const;

    [[nodiscard]] std::int32_t surfaceHeightAt(std::int32_t x, std::int32_t z, Samplers& samplers) const;

    static void recordStart(Chunk& chunk, const structure::StructureBoundingBox& clip, CandidateKey origin,
                            const std::vector<structure::PlacedStructurePiece>& pieces);

    OverworldNoiseRouter router_;
    structure::StructurePlacementGrid grid_;
    BiomeAt biomeAt_;
    std::set<std::string, std::less<>> outpostBiomes_;
    PoolLookup poolLookup_;
    structure::PoolTemplateLoader templateLoader_;
    mutable std::mutex mutex_;
    std::map<CandidateKey, std::vector<structure::PlacedStructurePiece>> cache_;
};

} // namespace basalt::server::world::terrain

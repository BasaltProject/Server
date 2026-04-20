#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain {

/// Where a ruined portal sits relative to the ground, which decides almost everything else.
enum class RuinedPortalPlacement : std::uint8_t {
    OnLandSurface,
    PartlyBuried,
    OnOceanFloor,
    InMountain,
    Underground,
    InNether,
};

/// parity: `RuinedPortalStructure.Setup`, one weighted way of placing a portal.
struct RuinedPortalSetup {
    RuinedPortalPlacement placement = RuinedPortalPlacement::OnLandSurface;
    float airPocketProbability = 0.0F;
    float mossiness = 0.0F;
    bool overgrown = false;
    bool vines = false;
    bool canBeCold = false;
    bool replaceWithBlackstone = false;
    float weight = 1.0F;
};

/// One of the seven ruined portal structures, with the biomes it may start in.
struct RuinedPortalVariant {
    std::string name;
    std::set<std::string, std::less<>> biomes;
    std::vector<RuinedPortalSetup> setups;
};

/// Overworld ruined portals. parity: the `ruined_portals` set is a plain `random_spread`, spacing 40,
/// separation 15, salt 34222645. The seven variants share the cell by weight, first accepted wins.
class RuinedPortalStructureGenerator {
public:
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using HeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;
    using IsSolidAt = std::function<bool(std::int32_t, std::int32_t, std::int32_t)>;

    RuinedPortalStructureGenerator(std::vector<RuinedPortalVariant> variants, TemplateOf templateOf, BiomeAt biomeAt,
                                   HeightAt surfaceHeightAt, HeightAt oceanFloorAt, IsSolidAt isSolidAt);

    /// Writes into `chunk` whatever of a nearby ruined portal lands there.
    void generateInto(Chunk& chunk, std::int64_t seed);

    /// The names of the seven structures this generator makes, which is what `/place structure`
    /// accepts for a ruined portal.
    [[nodiscard]] std::vector<std::string> variantNames() const;

    /// parity: `PlaceCommand.placeStructure`: the bounds of the named variant started at this chunk
    /// with the grid and biome checks skipped, or nothing when its template draw found nothing.
    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, std::string_view variant) const;

    /// Writes the part of the named variant started at (chunkX, chunkZ) that lands in `chunk`, like
    /// the worldgen pass does, but without the grid and biome checks and without recording a start.
    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                 std::string_view variant) const;

private:
    struct Portal {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        /// Which of the seven variants this is, which decides its index within the step.
        std::string variantName;
        RuinedPortalSetup setup;
        const structure::StructureTemplate* piece = nullptr;
        structure::StructurePos origin;
        structure::StructureRotation rotation = structure::StructureRotation::None;
        structure::StructureMirror mirror = structure::StructureMirror::None;
        structure::StructurePos pivot;
        structure::StructureBoundingBox box;
        bool airPocket = false;
        bool cold = false;
    };

    void place(const Portal& portal, Chunk& chunk, const structure::StructureBoundingBox& clip,
               std::int64_t seed) const;

    /// parity: `RuinedPortalPiece.maybeAddVines`.
    static void maybeAddVines(Chunk& chunk, random::RandomSource& random, const structure::StructureBoundingBox& clip,
                              std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: `RuinedPortalPiece.maybeAddLeavesAbove`. The draw happens either way.
    static void maybeAddLeavesAbove(Chunk& chunk, random::RandomSource& random,
                                    const structure::StructureBoundingBox& clip, std::int32_t x, std::int32_t y,
                                    std::int32_t z);

    /// parity: `RuinedPortalPiece.addNetherrackDripColumnsBelowPortal`.
    static void dripColumnsBelowPortal(const Portal& portal, Chunk& chunk, const structure::StructureBoundingBox& clip,
                                       random::RandomSource& random);

    /// parity: `RuinedPortalPiece.spreadNetherrack`, the apron of netherrack around the portal that
    /// fades out with taxicab distance from its centre.
    void spreadNetherrack(const Portal& portal, Chunk& chunk, const structure::StructureBoundingBox& clip,
                          random::RandomSource& random) const;

    static void addDripColumn(Chunk& chunk, random::RandomSource& random, const Portal& portal,
                              const structure::StructureBoundingBox& clip, std::int32_t x, std::int32_t startY,
                              std::int32_t z);

    static void placeNetherrackOrMagma(Chunk& chunk, random::RandomSource& random, const Portal& portal, std::int32_t x,
                                       std::int32_t y, std::int32_t z);

    /// parity: `RuinedPortalPiece.getHeightMapType`. Only the ocean floor variant reads
    /// `OCEAN_FLOOR_WG`; every other placement, the swamp one included, reads `WORLD_SURFACE_WG`,
    /// which over water is the surface of the water.
    [[nodiscard]] std::int32_t columnHeight(RuinedPortalPlacement placement, std::int32_t x, std::int32_t z) const;

    /// The `_WG` height as the level would answer it. Inside the chunk being written that is the
    /// frozen snapshot; outside it the apron never places anything, so the plain column stands in.
    [[nodiscard]] std::int32_t frozenHeight(const Chunk& chunk, RuinedPortalPlacement placement, std::int32_t x,
                                            std::int32_t z) const;

    /// Shared rather than borrowed: the cache is cleared when it fills, and a pointer into it would
    /// dangle the moment another worker triggered that while this one was still reading. A null
    /// pointer for a key that is present means the chunk was checked and holds no portal.
    [[nodiscard]] std::shared_ptr<const Portal> portalAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: `ChunkGenerator`'s retry loop. A variant is drawn by weight, its whole generation
    /// point computed, and only then is the biome checked, at that point rather than at the chunk. A
    /// refused variant is dropped and the next drawn, each attempt starting from the same state.
    [[nodiscard]] std::optional<Portal> assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: `RuinedPortalStructure.findGenerationPoint`, whose every draw is in this order.
    [[nodiscard]] std::optional<Portal> generationPoint(std::int32_t chunkX, std::int32_t chunkZ,
                                                        const RuinedPortalVariant& variant, std::int64_t seed) const;

    /// The named variant's generation point at this chunk: what `/place structure` places, with no
    /// variant draw and no biome test.
    [[nodiscard]] std::optional<Portal> placementStart(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                                       std::string_view variant) const;

    [[nodiscard]] std::int32_t suitableY(random::RandomSource& random, RuinedPortalPlacement placement, bool airPocket,
                                         std::int32_t surfaceY, std::int32_t ySpan,
                                         const structure::StructureBoundingBox& box) const;

    std::vector<RuinedPortalVariant> variants_;
    TemplateOf templateOf_;
    BiomeAt biomeAt_;
    HeightAt surfaceHeightAt_;
    HeightAt oceanFloorAt_;
    IsSolidAt isSolidAt_;
    structure::StructurePlacementGrid grid_;
    mutable std::mutex mutex_;
    std::map<std::int64_t, std::shared_ptr<const Portal>> placed_;
};

} // namespace basalt::server::world::terrain

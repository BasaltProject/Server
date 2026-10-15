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

struct RuinedPortalVariant {
    std::string name;
    std::set<std::string, std::less<>> biomes;
    std::vector<RuinedPortalSetup> setups;
};

/// Overworld ruined portals. parity: the `ruined_portals` set is a plain `random_spread`, spacing 40.
class RuinedPortalStructureGenerator {
public:
    using TemplateOf = std::function<const structure::StructureTemplate*(std::string_view)>;
    using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;
    using HeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;
    using IsSolidAt = std::function<bool(std::int32_t, std::int32_t, std::int32_t)>;

    RuinedPortalStructureGenerator(std::vector<RuinedPortalVariant> variants, TemplateOf templateOf, BiomeAt biomeAt,
                                   HeightAt surfaceHeightAt, HeightAt oceanFloorAt, IsSolidAt isSolidAt);

    void generateInto(Chunk& chunk, std::int64_t seed);

    [[nodiscard]] std::vector<std::string> variantNames() const;

    /// parity: `PlaceCommand.placeStructure`.
    [[nodiscard]] std::optional<structure::StructureBoundingBox>
    placeBoundsAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, std::string_view variant) const;

    void placeAt(Chunk& chunk, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                 std::string_view variant) const;

private:
    struct Portal {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
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

    /// parity: `RuinedPortalPiece.spreadNetherrack`.
    void spreadNetherrack(const Portal& portal, Chunk& chunk, const structure::StructureBoundingBox& clip,
                          random::RandomSource& random) const;

    static void addDripColumn(Chunk& chunk, random::RandomSource& random, const Portal& portal,
                              const structure::StructureBoundingBox& clip, std::int32_t x, std::int32_t startY,
                              std::int32_t z);

    static void placeNetherrackOrMagma(Chunk& chunk, random::RandomSource& random, const Portal& portal, std::int32_t x,
                                       std::int32_t y, std::int32_t z);

    /// parity: `RuinedPortalPiece.getHeightMapType`. Only the ocean floor variant reads.
    [[nodiscard]] std::int32_t columnHeight(RuinedPortalPlacement placement, std::int32_t x, std::int32_t z) const;

    [[nodiscard]] std::int32_t frozenHeight(const Chunk& chunk, RuinedPortalPlacement placement, std::int32_t x,
                                            std::int32_t z) const;

    [[nodiscard]] std::shared_ptr<const Portal> portalAt(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

    /// parity: `ChunkGenerator`'s retry loop.
    [[nodiscard]] std::optional<Portal> assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) const;

    /// parity: `RuinedPortalStructure.findGenerationPoint`.
    [[nodiscard]] std::optional<Portal> generationPoint(std::int32_t chunkX, std::int32_t chunkZ,
                                                        const RuinedPortalVariant& variant, std::int64_t seed) const;

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

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTypes.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

using FoliagePositions = std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>;

/// parity: the `net.minecraft.world.level.levelgen.feature.foliageplacers` shapes.
class FoliagePlacer {
public:
    /// parity: `BlobFoliagePlacer` (oak, birch, jungle).
    [[nodiscard]] static FoliagePlacer blob(IntProvider radius, IntProvider offset, std::int32_t height);

    /// parity: `BushFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer bush(IntProvider radius, IntProvider offset, std::int32_t height);

    /// parity: `MegaJungleFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer megaJungle(IntProvider radius, IntProvider offset, std::int32_t height);

    /// parity: `FancyFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer fancy(IntProvider radius, IntProvider offset, std::int32_t height);

    /// parity: `AcaciaFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer acacia(IntProvider radius, IntProvider offset);

    /// parity: `SpruceFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer spruce(IntProvider radius, IntProvider offset, IntProvider trunkHeight);

    /// parity: `PineFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer pine(IntProvider radius, IntProvider offset, IntProvider height);

    /// parity: `DarkOakFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer darkOak(IntProvider radius, IntProvider offset);

    /// parity: `MegaPineFoliagePlacer`, a tapered crown with a jagged edge every other row.
    [[nodiscard]] static FoliagePlacer megaPine(IntProvider radius, IntProvider offset, IntProvider crownHeight);

    /// parity: `RandomSpreadFoliagePlacer` (mangrove): scattered attempts in a box, no rows.
    [[nodiscard]] static FoliagePlacer randomSpread(IntProvider radius, IntProvider offset, IntProvider foliageHeight,
                                                    std::int32_t leafPlacementAttempts);

    [[nodiscard]] static FoliagePlacer weightedRandomSpread(IntProvider radius, IntProvider offset,
                                                            IntProvider foliageHeight,
                                                            std::int32_t leafPlacementAttempts,
                                                            std::vector<std::pair<std::int32_t, std::int32_t>> states);

    /// parity: `CherryFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer cherry(IntProvider radius, IntProvider offset, IntProvider height,
                                              float wideBottomLayerHoleChance, float cornerHoleChance,
                                              float hangingLeavesChance, float hangingLeavesExtensionChance);

    /// parity: `PoplarFoliagePlacer`.
    [[nodiscard]] static FoliagePlacer poplar(IntProvider radius, IntProvider offset, IntProvider height,
                                              float sideHoleChance);

    [[nodiscard]] std::int32_t foliageRadius(random::RandomSource& random, std::int32_t trunkHeight) const;

    [[nodiscard]] std::int32_t foliageHeight(random::RandomSource& random, std::int32_t treeHeight) const;

    [[nodiscard]] std::int32_t offsetOf(random::RandomSource& random) const;

    void createFoliage(Chunk& chunk, random::RandomSource& random, std::int32_t foliageStateId,
                       const FoliageAttachment& attachment, std::int32_t foliageHeight, std::int32_t leafRadius,
                       std::int32_t offset, FoliagePositions& foliagePositions, std::int32_t trunkStateId = 0) const;

private:
    enum class Kind : std::uint8_t {
        Blob,
        Bush,
        MegaJungle,
        Fancy,
        Acacia,
        Spruce,
        Pine,
        DarkOak,
        MegaPine,
        RandomSpread,
        WeightedRandomSpread,
        Cherry,
        Poplar,
    };

    [[nodiscard]] bool shouldSkipLocation(random::RandomSource& random, std::int32_t dx, std::int32_t y,
                                          std::int32_t dz, std::int32_t currentRadius, bool doubleTrunk) const;

    /// parity: `FoliagePlacer.shouldSkipLocationSigned`.
    [[nodiscard]] bool shouldSkipLocationSigned(random::RandomSource& random, std::int32_t dx, std::int32_t y,
                                                std::int32_t dz, std::int32_t currentRadius, bool doubleTrunk) const;

    void placeLeavesRow(Chunk& chunk, random::RandomSource& random, std::int32_t foliageStateId, std::int32_t originX,
                        std::int32_t originY, std::int32_t originZ, std::int32_t currentRadius, std::int32_t y,
                        bool doubleTrunk, FoliagePositions* foliagePositions = nullptr) const;

    void placeLeavesRowWithHangingLeavesBelow(Chunk& chunk, random::RandomSource& random, std::int32_t foliageStateId,
                                              std::int32_t originX, std::int32_t originY, std::int32_t originZ,
                                              std::int32_t currentRadius, std::int32_t y, bool doubleTrunk,
                                              float hangingLeavesChance, float hangingLeavesExtensionChance,
                                              FoliagePositions& foliagePositions) const;

    void createPoplarFoliage(Chunk& chunk, random::RandomSource& random, std::int32_t foliageStateId,
                             std::int32_t trunkStateId, const FoliageAttachment& attachment, std::int32_t foliageHeight,
                             std::int32_t leafRadius, std::int32_t offset, FoliagePositions& foliagePositions) const;

    bool tryPlaceExtension(Chunk& chunk, random::RandomSource& random, std::int32_t foliageStateId, float chance,
                           std::int32_t logX, std::int32_t logY, std::int32_t logZ, std::int32_t x, std::int32_t y,
                           std::int32_t z, FoliagePositions& foliagePositions) const;

    Kind kind_ = Kind::Blob;
    IntProvider radius_;
    IntProvider offset_;
    IntProvider heightProvider_;
    IntProvider trunkHeight_;
    std::int32_t height_ = 0;
    std::int32_t leafPlacementAttempts_ = 0;
    float wideBottomLayerHoleChance_ = 0.0F;
    float cornerHoleChance_ = 0.0F;
    float hangingLeavesChance_ = 0.0F;
    float hangingLeavesExtensionChance_ = 0.0F;
    float sideHoleChance_ = 0.0F;
    std::shared_ptr<const std::vector<std::pair<std::int32_t, std::int32_t>>> weightedStates_;
    std::int32_t totalWeight_ = 0;
};

} // namespace basalt::server::world::terrain::decoration::tree

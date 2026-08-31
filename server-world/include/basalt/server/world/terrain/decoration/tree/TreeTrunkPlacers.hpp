#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/IntProvider.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTypes.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: the `net.minecraft.world.level.levelgen.feature.trunkplacers` shapes.
///
/// One kind-tagged value type rather than a hierarchy: this build has RTTI off, the variants differ
/// by at most three extra fields, and a tree config copies its placer around by value.
class TrunkPlacer {
public:
    /// parity: `StraightTrunkPlacer`, one column of logs.
    [[nodiscard]] static TrunkPlacer straight(std::int32_t baseHeight, std::int32_t heightRandA,
                                              std::int32_t heightRandB);

    /// parity: `BendingTrunkPlacer`, the azalea tree grown out of a root system.
    [[nodiscard]] static TrunkPlacer bending(std::int32_t baseHeight, std::int32_t heightRandA,
                                             std::int32_t heightRandB, std::int32_t minHeightForLeaves,
                                             IntProvider bendLength, std::int32_t belowTrunkStateId);

    /// parity: `FancyTrunkPlacer`, the large oak with probed branch limbs.
    [[nodiscard]] static TrunkPlacer fancy(std::int32_t baseHeight, std::int32_t heightRandA, std::int32_t heightRandB);

    /// parity: `ForkingTrunkPlacer`, the acacia.
    [[nodiscard]] static TrunkPlacer forking(std::int32_t baseHeight, std::int32_t heightRandA,
                                             std::int32_t heightRandB);

    /// parity: `DarkOakTrunkPlacer`, a 2x2 trunk with random side branches.
    [[nodiscard]] static TrunkPlacer darkOak(std::int32_t baseHeight, std::int32_t heightRandA,
                                             std::int32_t heightRandB);

    /// parity: `GiantTrunkPlacer`, the solid 2x2 trunk of the mega spruce and pine.
    [[nodiscard]] static TrunkPlacer giant(std::int32_t baseHeight, std::int32_t heightRandA, std::int32_t heightRandB);

    /// parity: `MegaJungleTrunkPlacer`, a giant trunk plus high lateral branches.
    [[nodiscard]] static TrunkPlacer megaJungle(std::int32_t baseHeight, std::int32_t heightRandA,
                                                std::int32_t heightRandB);

    /// parity: `TrunkPlacer.getTreeHeight`. Two draws even when either bound is zero.
    [[nodiscard]] std::int32_t treeHeight(random::RandomSource& random) const;

    [[nodiscard]] std::vector<FoliageAttachment> placeTrunk(Chunk& chunk, random::RandomSource& random,
                                                            std::int32_t treeHeight, std::int32_t originX,
                                                            std::int32_t originY, std::int32_t originZ,
                                                            std::int32_t trunkStateId) const;

    /// parity: `TrunkPlacer.isFree`, which also accepts a log already standing there.
    [[nodiscard]] bool isFree(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    enum class Kind : std::uint8_t { Straight, Bending, Fancy, Forking, DarkOak, Giant, MegaJungle };

    /// parity: `TrunkPlacer.validTreePos`. Kind-dispatched because specialized placers widen the
    /// accepted blocks rather than replacing the rule.
    [[nodiscard]] bool validTreePos(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) const;

    bool placeLog(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t trunkStateId) const;

    void placeLogIfFree(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t trunkStateId) const;

    [[nodiscard]] std::vector<FoliageAttachment> placeGiant(Chunk& chunk, std::int32_t treeHeight, std::int32_t originX,
                                                            std::int32_t originY, std::int32_t originZ,
                                                            std::int32_t trunkStateId) const;

    /// The placing form ignores the result, so this is deliberately not `[[nodiscard]]`.
    bool makeLimb(Chunk& chunk, std::int32_t startX, std::int32_t startY, std::int32_t startZ, std::int32_t endX,
                  std::int32_t endY, std::int32_t endZ, bool checkOnly, std::int32_t trunkStateId) const;

    Kind kind_ = Kind::Straight;
    std::int32_t baseHeight_ = 0;
    std::int32_t heightRandA_ = 0;
    std::int32_t heightRandB_ = 0;
    std::int32_t minHeightForLeaves_ = 0;
    IntProvider bendLength_;
    std::int32_t belowTrunkStateId_ = 0;
};

} // namespace basalt::server::world::terrain::decoration::tree

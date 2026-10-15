#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTrunkPlacers.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

constexpr std::int32_t kAssumedFoliageHeight = 5;
constexpr double kTrunkHeightScale = 0.618;
constexpr double kClusterDensityMagic = 1.382;
constexpr double kBranchSlope = 0.381;
constexpr double kBranchLengthMagic = 0.328;

using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// parity: block tag `mangrove_logs_can_grow_through`.
const PathSet& mangroveLogGrowThroughPaths() {
    static const PathSet paths = {"mud",          "muddy_mangrove_roots", "mangrove_roots", "mangrove_leaves",
                                  "mangrove_log", "mangrove_propagule",   "moss_carpet",    "vine"};
    return paths;
}

const PathSet& leafBlockPaths() {
    static const PathSet paths = {
        "oak_leaves",        "birch_leaves",        "spruce_leaves",           "jungle_leaves",
        "acacia_leaves",     "dark_oak_leaves",     "pale_oak_leaves",         "mangrove_leaves",
        "cherry_leaves",     "azalea_leaves",       "flowering_azalea_leaves", "orange_poplar_leaves",
        "red_poplar_leaves", "yellow_poplar_leaves"};
    return paths;
}

[[nodiscard]] bool isAirOrLeaves(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    const std::int32_t stateId = chunk.decorationBlockStateId(x, y, z);
    return registry::blocks::isAir(stateId) || leafBlockPaths().find(pathOf(stateId)) != leafBlockPaths().end();
}

/// parity: `FancyTrunkPlacer.treeShape` computes the square root before deciding it was needed (discarded NaN).
[[nodiscard]] float treeShape(const std::int32_t height, const std::int32_t y) {
    if (static_cast<float>(y) < static_cast<float>(height) * 0.3F) {
        return -1.0F;
    }
    const float radius = static_cast<float>(height) / 2.0F;
    const float adjacent = radius - static_cast<float>(y);
    if (std::abs(adjacent) >= radius) {
        return adjacent == 0.0F ? radius * 0.5F : 0.0F;
    }
    const float distance = adjacent == 0.0F ? radius : std::sqrt(radius * radius - adjacent * adjacent);
    return distance * 0.5F;
}

[[nodiscard]] bool trimBranches(const std::int32_t height, const std::int32_t localY) {
    return static_cast<double>(localY) >= static_cast<double>(height) * 0.2;
}

/// parity: `FancyTrunkPlacer.getLogAxis`.
[[nodiscard]] std::int32_t axisStateId(const std::int32_t defaultStateId, const std::int32_t startX,
                                       const std::int32_t startZ, const std::int32_t x, const std::int32_t z) {
    const std::int32_t xDifference = std::abs(x - startX);
    const std::int32_t zDifference = std::abs(z - startZ);
    const std::int32_t largest = std::max(xDifference, zDifference);
    if (largest == 0) {
        return defaultStateId;
    }
    return xDifference == largest ? defaultStateId - 1 : defaultStateId + 1;
}

/// parity: `Direction.allShuffled` over down.
[[nodiscard]] std::vector<HorizontalDirection> shuffledHorizontalDirections(random::RandomSource& random) {
    struct Direction {
        HorizontalDirection step;
        bool horizontal;
    };

    std::array<Direction, 6> all{Direction{{0, 0}, false}, Direction{{0, 0}, false}, Direction{{0, -1}, true},
                                 Direction{{0, 1}, true},  Direction{{-1, 0}, true}, Direction{{1, 0}, true}};
    for (std::int32_t size = static_cast<std::int32_t>(all.size()); size > 1; --size) {
        std::swap(all[static_cast<std::size_t>(size - 1)], all[static_cast<std::size_t>(random.nextInt(size))]);
    }
    std::vector<HorizontalDirection> horizontal;
    for (const Direction& direction : all) {
        if (direction.horizontal) {
            horizontal.push_back(direction.step);
        }
    }
    return horizontal;
}

struct FancyFoliageCoords {
    FoliageAttachment attachment;
    std::int32_t branchBase = 0;
};

} // namespace

TrunkPlacer TrunkPlacer::straight(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                  const std::int32_t heightRandB) {
    TrunkPlacer placer;
    placer.kind_ = Kind::Straight;
    placer.baseHeight_ = baseHeight;
    placer.heightRandA_ = heightRandA;
    placer.heightRandB_ = heightRandB;
    return placer;
}

TrunkPlacer TrunkPlacer::bending(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                 const std::int32_t heightRandB, const std::int32_t minHeightForLeaves,
                                 IntProvider bendLength, const std::int32_t belowTrunkStateId) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Bending;
    placer.minHeightForLeaves_ = minHeightForLeaves;
    placer.bendLength_ = std::move(bendLength);
    placer.belowTrunkStateId_ = belowTrunkStateId;
    return placer;
}

TrunkPlacer TrunkPlacer::fancy(const std::int32_t baseHeight, const std::int32_t heightRandA,
                               const std::int32_t heightRandB) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Fancy;
    return placer;
}

TrunkPlacer TrunkPlacer::forking(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                 const std::int32_t heightRandB) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Forking;
    return placer;
}

TrunkPlacer TrunkPlacer::darkOak(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                 const std::int32_t heightRandB) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::DarkOak;
    return placer;
}

TrunkPlacer TrunkPlacer::giant(const std::int32_t baseHeight, const std::int32_t heightRandA,
                               const std::int32_t heightRandB) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Giant;
    return placer;
}

TrunkPlacer TrunkPlacer::megaJungle(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                    const std::int32_t heightRandB) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::MegaJungle;
    return placer;
}

TrunkPlacer TrunkPlacer::upwardsBranching(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                          const std::int32_t heightRandB, IntProvider extraBranchSteps,
                                          const float placeBranchPerLogProbability, IntProvider extraBranchLength) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::UpwardsBranching;
    placer.extraBranchSteps_ = std::move(extraBranchSteps);
    placer.placeBranchPerLogProbability_ = placeBranchPerLogProbability;
    placer.extraBranchLength_ = std::move(extraBranchLength);
    return placer;
}

TrunkPlacer TrunkPlacer::cherry(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                const std::int32_t heightRandB, IntProvider branchCount,
                                IntProvider branchHorizontalLength, const std::int32_t branchStartOffsetMin,
                                const std::int32_t branchStartOffsetMax, IntProvider branchEndOffsetFromTop) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Cherry;
    placer.branchCount_ = std::move(branchCount);
    placer.branchHorizontalLength_ = std::move(branchHorizontalLength);
    placer.branchStartOffsetFromTop_ = IntProvider::uniform(branchStartOffsetMin, branchStartOffsetMax);
    placer.secondBranchStartOffsetFromTop_ = IntProvider::uniform(branchStartOffsetMin, branchStartOffsetMax - 1);
    placer.branchEndOffsetFromTop_ = std::move(branchEndOffsetFromTop);
    return placer;
}

TrunkPlacer TrunkPlacer::poplar(const std::int32_t baseHeight, const std::int32_t heightRandA,
                                const std::int32_t heightRandB, IntProvider trunkHeightAboveBranches,
                                IntProvider branchAmount) {
    TrunkPlacer placer = straight(baseHeight, heightRandA, heightRandB);
    placer.kind_ = Kind::Poplar;
    placer.trunkHeightAboveBranches_ = std::move(trunkHeightAboveBranches);
    placer.branchAmount_ = std::move(branchAmount);
    return placer;
}

std::int32_t TrunkPlacer::treeHeight(random::RandomSource& random) const {
    return baseHeight_ + random.nextInt(heightRandA_ + 1) + random.nextInt(heightRandB_ + 1);
}

bool TrunkPlacer::validTreePos(const Chunk& chunk, const std::int32_t x, const std::int32_t y,
                               const std::int32_t z) const {
    if (isValidTreePos(chunk, x, y, z)) {
        return true;
    }
    if (kind_ != Kind::UpwardsBranching) {
        return false;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    return mangroveLogGrowThroughPaths().contains(pathOf(chunk.decorationBlockStateId(x, y, z)));
}

bool TrunkPlacer::isFree(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) const {
    return validTreePos(chunk, x, y, z) || treeBlockRules::isLogState(chunk.decorationBlockStateId(x, y, z));
}

bool TrunkPlacer::placeLog(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           const std::int32_t trunkStateId) const {
    if (!validTreePos(chunk, x, y, z)) {
        return false;
    }
    setBlock(chunk, x, y, z, trunkStateId);
    return true;
}

void TrunkPlacer::placeLogIfFree(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                 const std::int32_t trunkStateId) const {
    if (isFree(chunk, x, y, z)) {
        placeLog(chunk, x, y, z, trunkStateId);
    }
}

bool TrunkPlacer::makeLimb(Chunk& chunk, const std::int32_t startX, const std::int32_t startY,
                           const std::int32_t startZ, const std::int32_t endX, const std::int32_t endY,
                           const std::int32_t endZ, const bool checkOnly, const std::int32_t trunkStateId) const {
    if (checkOnly && startX == endX && startY == endY && startZ == endZ) {
        return true;
    }
    const std::int32_t deltaX = endX - startX;
    const std::int32_t deltaY = endY - startY;
    const std::int32_t deltaZ = endZ - startZ;
    const std::int32_t steps = std::max(std::abs(deltaX), std::max(std::abs(deltaY), std::abs(deltaZ)));
    if (steps == 0) {
        if (!checkOnly) {
            placeLog(chunk, startX, startY, startZ, trunkStateId);
        }
        return true;
    }
    const float dx = static_cast<float>(deltaX) / static_cast<float>(steps);
    const float dy = static_cast<float>(deltaY) / static_cast<float>(steps);
    const float dz = static_cast<float>(deltaZ) / static_cast<float>(steps);
    for (std::int32_t step = 0; step <= steps; ++step) {
        const std::int32_t x = startX + static_cast<std::int32_t>(std::floor(0.5F + static_cast<float>(step) * dx));
        const std::int32_t y = startY + static_cast<std::int32_t>(std::floor(0.5F + static_cast<float>(step) * dy));
        const std::int32_t z = startZ + static_cast<std::int32_t>(std::floor(0.5F + static_cast<float>(step) * dz));
        if (checkOnly) {
            if (!isFree(chunk, x, y, z)) {
                return false;
            }
        } else {
            placeLog(chunk, x, y, z, axisStateId(trunkStateId, startX, startZ, x, z));
        }
    }
    return true;
}

std::vector<FoliageAttachment> TrunkPlacer::placeGiant(Chunk& chunk, const std::int32_t treeHeight,
                                                       const std::int32_t originX, const std::int32_t originY,
                                                       const std::int32_t originZ,
                                                       const std::int32_t trunkStateId) const {
    placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
    placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ);
    placeBelowTrunkBlock(chunk, originX, originY - 1, originZ + 1);
    placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ + 1);
    for (std::int32_t hh = 0; hh < treeHeight; ++hh) {
        placeLogIfFree(chunk, originX, originY + hh, originZ, trunkStateId);
        if (hh < treeHeight - 1) {
            placeLogIfFree(chunk, originX + 1, originY + hh, originZ, trunkStateId);
            placeLogIfFree(chunk, originX + 1, originY + hh, originZ + 1, trunkStateId);
            placeLogIfFree(chunk, originX, originY + hh, originZ + 1, trunkStateId);
        }
    }
    return {FoliageAttachment{
        .x = originX, .y = originY + treeHeight, .z = originZ, .radiusOffset = 0, .doubleTrunk = true}};
}

std::vector<FoliageAttachment> TrunkPlacer::placeTrunk(Chunk& chunk, random::RandomSource& random,
                                                       const std::int32_t treeHeight, const std::int32_t originX,
                                                       const std::int32_t originY, const std::int32_t originZ,
                                                       const std::int32_t trunkStateId) const {
    switch (kind_) {
    case Kind::Straight: {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
        for (std::int32_t y = 0; y < treeHeight; ++y) {
            placeLog(chunk, originX, originY + y, originZ, trunkStateId);
        }
        return {FoliageAttachment{.x = originX, .y = originY + treeHeight, .z = originZ}};
    }
    case Kind::Bending: {
        const HorizontalDirection direction = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
        const std::int32_t logHeight = treeHeight - 1;
        std::int32_t x = originX;
        std::int32_t y = originY;
        std::int32_t z = originZ;
        setBlock(chunk, x, y - 1, z, belowTrunkStateId_);
        std::vector<FoliageAttachment> attachments;

        for (std::int32_t level = 0; level <= logHeight; ++level) {
            // parity: the bend decision draws every level, even the ones it cannot reach.
            if (level + 1 >= logHeight + random.nextInt(2)) {
                x += direction.dx;
                z += direction.dz;
            }
            placeLog(chunk, x, y, z, trunkStateId);
            if (level >= minHeightForLeaves_) {
                attachments.push_back(FoliageAttachment{.x = x, .y = y, .z = z});
            }
            y++;
        }

        const std::int32_t bend = bendLength_.sample(random) + 1;
        for (std::int32_t step = 0; step < bend; ++step) {
            placeLog(chunk, x, y, z, trunkStateId);
            attachments.push_back(FoliageAttachment{.x = x, .y = y, .z = z});
            x += direction.dx;
            z += direction.dz;
        }
        return attachments;
    }
    case Kind::Fancy: {
        const std::int32_t height = treeHeight + 2;
        const std::int32_t trunkHeight =
            static_cast<std::int32_t>(std::floor(static_cast<double>(height) * kTrunkHeightScale));
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
        // parity: `min`, not `max`, so the fancy oak caps at one cluster per level.
        const std::int32_t clustersPerY = std::min(
            1, static_cast<std::int32_t>(std::floor(kClusterDensityMagic + (static_cast<double>(height) / 13.0) *
                                                                               (static_cast<double>(height) / 13.0))));
        const std::int32_t trunkTop = originY + trunkHeight;
        std::int32_t relativeY = height - kAssumedFoliageHeight;
        std::vector<FancyFoliageCoords> foliageCoords{
            FancyFoliageCoords{FoliageAttachment{.x = originX, .y = originY + relativeY, .z = originZ}, trunkTop}};

        while (relativeY >= 0) {
            const float shape = treeShape(height, relativeY);
            if (shape >= 0.0F) {
                for (std::int32_t cluster = 0; cluster < clustersPerY; ++cluster) {
                    const double radius =
                        static_cast<double>(shape) * (static_cast<double>(random.nextFloat()) + kBranchLengthMagic);
                    const double angle = static_cast<double>(random.nextFloat()) * 2.0 * std::numbers::pi;
                    const std::int32_t checkStartX =
                        originX + static_cast<std::int32_t>(std::floor(radius * std::sin(angle) + 0.5));
                    const std::int32_t checkStartY = originY + relativeY - 1;
                    const std::int32_t checkStartZ =
                        originZ + static_cast<std::int32_t>(std::floor(radius * std::cos(angle) + 0.5));
                    if (makeLimb(chunk, checkStartX, checkStartY, checkStartZ, checkStartX,
                                 checkStartY + kAssumedFoliageHeight, checkStartZ, true, trunkStateId)) {
                        const std::int32_t dx = originX - checkStartX;
                        const std::int32_t dz = originZ - checkStartZ;
                        const double branchHeight = static_cast<double>(checkStartY) -
                                                    std::sqrt(static_cast<double>(dx * dx + dz * dz)) * kBranchSlope;
                        const std::int32_t branchTop = branchHeight > static_cast<double>(trunkTop)
                                                           ? trunkTop
                                                           : static_cast<std::int32_t>(branchHeight);
                        if (makeLimb(chunk, originX, branchTop, originZ, checkStartX, checkStartY, checkStartZ, true,
                                     trunkStateId)) {
                            foliageCoords.push_back(FancyFoliageCoords{
                                FoliageAttachment{.x = checkStartX, .y = checkStartY, .z = checkStartZ}, branchTop});
                        }
                    }
                }
            }
            relativeY--;
        }

        makeLimb(chunk, originX, originY, originZ, originX, originY + trunkHeight, originZ, false, trunkStateId);
        for (const FancyFoliageCoords& coord : foliageCoords) {
            if (coord.attachment.x == originX && coord.attachment.y == coord.branchBase &&
                coord.attachment.z == originZ) {
                continue;
            }
            if (trimBranches(height, coord.branchBase - originY)) {
                makeLimb(chunk, originX, coord.branchBase, originZ, coord.attachment.x, coord.attachment.y,
                         coord.attachment.z, false, trunkStateId);
            }
        }

        std::vector<FoliageAttachment> attachments;
        for (const FancyFoliageCoords& coord : foliageCoords) {
            if (trimBranches(height, coord.branchBase - originY)) {
                attachments.push_back(coord.attachment);
            }
        }
        return attachments;
    }
    case Kind::Forking: {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
        std::vector<FoliageAttachment> attachments;
        const HorizontalDirection leanDirection = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
        const std::int32_t leanHeight = treeHeight - random.nextInt(4) - 1;
        std::int32_t leanSteps = 3 - random.nextInt(3);
        std::int32_t tx = originX;
        std::int32_t tz = originZ;
        std::int32_t ey = std::numeric_limits<std::int32_t>::min();

        for (std::int32_t yo = 0; yo < treeHeight; ++yo) {
            const std::int32_t yy = originY + yo;
            if (yo >= leanHeight && leanSteps > 0) {
                tx += leanDirection.dx;
                tz += leanDirection.dz;
                leanSteps--;
            }
            if (placeLog(chunk, tx, yy, tz, trunkStateId)) {
                ey = yy + 1;
            }
        }
        if (ey != std::numeric_limits<std::int32_t>::min()) {
            attachments.push_back(FoliageAttachment{.x = tx, .y = ey, .z = tz, .radiusOffset = 1});
        }

        tx = originX;
        tz = originZ;
        const HorizontalDirection branchDirection = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
        if (!(branchDirection == leanDirection)) {
            std::int32_t yo = leanHeight - random.nextInt(2) - 1;
            std::int32_t branchSteps = 1 + random.nextInt(3);
            ey = std::numeric_limits<std::int32_t>::min();
            while (yo < treeHeight && branchSteps > 0) {
                if (yo >= 1) {
                    const std::int32_t yy = originY + yo;
                    tx += branchDirection.dx;
                    tz += branchDirection.dz;
                    if (placeLog(chunk, tx, yy, tz, trunkStateId)) {
                        ey = yy + 1;
                    }
                }
                yo++;
                branchSteps--;
            }
            if (ey != std::numeric_limits<std::int32_t>::min()) {
                attachments.push_back(FoliageAttachment{.x = tx, .y = ey, .z = tz});
            }
        }
        return attachments;
    }
    case Kind::DarkOak: {
        std::vector<FoliageAttachment> attachments;
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ);
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ + 1);
        placeBelowTrunkBlock(chunk, originX + 1, originY - 1, originZ + 1);
        const HorizontalDirection leanDirection = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
        const std::int32_t leanHeight = treeHeight - random.nextInt(4);
        std::int32_t leanSteps = 2 - random.nextInt(3);
        std::int32_t tx = originX;
        std::int32_t tz = originZ;
        const std::int32_t ey = originY + treeHeight - 1;

        for (std::int32_t dy = 0; dy < treeHeight; ++dy) {
            if (dy >= leanHeight && leanSteps > 0) {
                tx += leanDirection.dx;
                tz += leanDirection.dz;
                leanSteps--;
            }
            const std::int32_t yy = originY + dy;
            if (isAirOrLeaves(chunk, tx, yy, tz)) {
                placeLog(chunk, tx, yy, tz, trunkStateId);
                placeLog(chunk, tx + 1, yy, tz, trunkStateId);
                placeLog(chunk, tx, yy, tz + 1, trunkStateId);
                placeLog(chunk, tx + 1, yy, tz + 1, trunkStateId);
            }
        }
        attachments.push_back(FoliageAttachment{.x = tx, .y = ey, .z = tz, .radiusOffset = 0, .doubleTrunk = true});

        for (std::int32_t ox = -1; ox <= 2; ++ox) {
            for (std::int32_t oz = -1; oz <= 2; ++oz) {
                if ((ox < 0 || ox > 1 || oz < 0 || oz > 1) && random.nextInt(3) <= 0) {
                    const std::int32_t length = random.nextInt(3) + 2;
                    for (std::int32_t branchY = 0; branchY < length; ++branchY) {
                        placeLog(chunk, originX + ox, ey - branchY - 1, originZ + oz, trunkStateId);
                    }
                    attachments.push_back(FoliageAttachment{.x = originX + ox, .y = ey, .z = originZ + oz});
                }
            }
        }
        return attachments;
    }
    case Kind::UpwardsBranching:
        return placeUpwardsBranchingTrunk(chunk, random, treeHeight, originX, originY, originZ, trunkStateId);
    case Kind::Cherry:
        return placeCherryTrunk(chunk, random, treeHeight, originX, originY, originZ, trunkStateId);
    case Kind::Poplar: {
        placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
        const std::int32_t branchTop = treeHeight - trunkHeightAboveBranches_.sample(random);
        for (std::int32_t y = 0; y < treeHeight; ++y) {
            placeLog(chunk, originX, originY + y, originZ, trunkStateId);
            // parity: `Direction.allShuffled` runs every level, even where no branch grows.
            const std::vector<HorizontalDirection> directions = shuffledHorizontalDirections(random);
            if (branchTop - 1 != y) {
                continue;
            }
            const std::int32_t branches = branchAmount_.sample(random);
            for (std::int32_t index = 0; index < branches; ++index) {
                const HorizontalDirection direction = directions[static_cast<std::size_t>(index)];
                placeLog(chunk, originX + direction.dx, originY + y, originZ + direction.dz,
                         direction.dx != 0 ? trunkStateId - 1 : trunkStateId + 1);
            }
        }
        return {FoliageAttachment{.x = originX, .y = originY + branchTop, .z = originZ}};
    }
    case Kind::Giant:
        return placeGiant(chunk, treeHeight, originX, originY, originZ, trunkStateId);
    case Kind::MegaJungle: {
        std::vector<FoliageAttachment> attachments =
            placeGiant(chunk, treeHeight, originX, originY, originZ, trunkStateId);
        std::int32_t branchHeight = treeHeight - 2 - random.nextInt(4);
        while (branchHeight > treeHeight / 2) {
            const float angle = random.nextFloat() * static_cast<float>(std::numbers::pi * 2.0);
            std::int32_t branchX = 0;
            std::int32_t branchZ = 0;
            for (std::int32_t step = 0; step < 5; ++step) {
                // parity: this placer uses Mth's 65536-entry sine table, the fancy oak above uses exact sine.
                branchX = static_cast<std::int32_t>(1.5F + wire::common::mth::cos(angle) * static_cast<float>(step));
                branchZ = static_cast<std::int32_t>(1.5F + wire::common::mth::sin(angle) * static_cast<float>(step));
                placeLog(chunk, originX + branchX, originY + branchHeight - 3 + step / 2, originZ + branchZ,
                         trunkStateId);
            }
            attachments.push_back(FoliageAttachment{
                .x = originX + branchX, .y = originY + branchHeight, .z = originZ + branchZ, .radiusOffset = -2});
            branchHeight -= 2 + random.nextInt(4);
        }
        return attachments;
    }
    }
    return {};
}

} // namespace basalt::server::world::terrain::decoration::tree

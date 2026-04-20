#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTrunkPlacers.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

/// parity: `CherryTrunkPlacer` sideways log is the trunk state on a horizontal axis.
[[nodiscard]] std::int32_t horizontalAxisStateId(const std::int32_t stateId, const HorizontalDirection direction) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    properties["axis"] = direction.dx != 0 ? "x" : "z";
    return registry::blocks::stateIdOf(*block, properties).value_or(stateId);
}

} // namespace

std::vector<FoliageAttachment>
TrunkPlacer::placeUpwardsBranchingTrunk(Chunk& chunk, random::RandomSource& random, const std::int32_t treeHeight,
                                        const std::int32_t originX, const std::int32_t originY,
                                        const std::int32_t originZ, const std::int32_t trunkStateId) const {
    std::vector<FoliageAttachment> attachments;
    for (std::int32_t heightPos = 0; heightPos < treeHeight; ++heightPos) {
        const std::int32_t currentHeight = originY + heightPos;
        const bool placed = placeLog(chunk, originX, currentHeight, originZ, trunkStateId);
        if (placed && heightPos < treeHeight - 1 && random.nextFloat() < placeBranchPerLogProbability_) {
            const HorizontalDirection branchDir = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
            const std::int32_t branchLen = extraBranchLength_.sample(random);
            const std::int32_t branchPos = std::max(0, branchLen - extraBranchLength_.sample(random) - 1);
            const std::int32_t branchSteps = extraBranchSteps_.sample(random);
            placeBranch(chunk, treeHeight, trunkStateId, attachments, originX, originZ, currentHeight, branchDir,
                        branchPos, branchSteps);
        }
        if (heightPos == treeHeight - 1) {
            attachments.push_back(FoliageAttachment{.x = originX, .y = currentHeight + 1, .z = originZ});
        }
    }
    return attachments;
}

void TrunkPlacer::placeBranch(Chunk& chunk, const std::int32_t treeHeight, const std::int32_t trunkStateId,
                              std::vector<FoliageAttachment>& attachments, const std::int32_t originX,
                              const std::int32_t originZ, const std::int32_t currentHeight,
                              const HorizontalDirection branchDir, const std::int32_t branchPos,
                              const std::int32_t branchStepsInitial) const {
    std::int32_t branchSteps = branchStepsInitial;
    std::int32_t heightAlongBranch = currentHeight + branchPos;
    std::int32_t logX = originX;
    std::int32_t logZ = originZ;
    std::int32_t branchPlacementIndex = branchPos;
    while (branchPlacementIndex < treeHeight && branchSteps > 0) {
        if (branchPlacementIndex >= 1) {
            const std::int32_t placementHeight = currentHeight + branchPlacementIndex;
            logX += branchDir.dx;
            logZ += branchDir.dz;
            heightAlongBranch = placementHeight;
            if (placeLog(chunk, logX, placementHeight, logZ, trunkStateId)) {
                heightAlongBranch++;
            }
            attachments.push_back(FoliageAttachment{.x = logX, .y = placementHeight, .z = logZ});
        }
        branchPlacementIndex++;
        branchSteps--;
    }
    if (heightAlongBranch - currentHeight > 1) {
        attachments.push_back(FoliageAttachment{.x = logX, .y = heightAlongBranch, .z = logZ});
        attachments.push_back(FoliageAttachment{.x = logX, .y = heightAlongBranch - 2, .z = logZ});
    }
}

std::vector<FoliageAttachment> TrunkPlacer::placeCherryTrunk(Chunk& chunk, random::RandomSource& random,
                                                             const std::int32_t treeHeight, const std::int32_t originX,
                                                             const std::int32_t originY, const std::int32_t originZ,
                                                             const std::int32_t trunkStateId) const {
    placeBelowTrunkBlock(chunk, originX, originY - 1, originZ);
    const std::int32_t firstBranchOffset = std::max(0, treeHeight - 1 + branchStartOffsetFromTop_.sample(random));
    std::int32_t secondBranchOffset = std::max(0, treeHeight - 1 + secondBranchStartOffsetFromTop_.sample(random));
    if (secondBranchOffset >= firstBranchOffset) {
        secondBranchOffset++;
    }

    const std::int32_t branches = branchCount_.sample(random);
    const bool hasMiddleBranch = branches == 3;
    const bool hasBothSideBranches = branches >= 2;
    std::int32_t trunkHeight = firstBranchOffset + 1;
    if (hasMiddleBranch) {
        trunkHeight = treeHeight;
    } else if (hasBothSideBranches) {
        trunkHeight = std::max(firstBranchOffset, secondBranchOffset) + 1;
    }
    for (std::int32_t y = 0; y < trunkHeight; ++y) {
        placeLog(chunk, originX, originY + y, originZ, trunkStateId);
    }

    std::vector<FoliageAttachment> attachments;
    if (hasMiddleBranch) {
        attachments.push_back(FoliageAttachment{.x = originX, .y = originY + trunkHeight, .z = originZ});
    }
    const HorizontalDirection treeDirection = kHorizontalDirections[static_cast<std::size_t>(random.nextInt(4))];
    const std::int32_t sidewaysStateId = horizontalAxisStateId(trunkStateId, treeDirection);
    attachments.push_back(generateCherryBranch(chunk, random, treeHeight, originX, originY, originZ, trunkStateId,
                                               sidewaysStateId, treeDirection, firstBranchOffset,
                                               firstBranchOffset < trunkHeight - 1));
    if (hasBothSideBranches) {
        const HorizontalDirection opposite{.dx = -treeDirection.dx, .dz = -treeDirection.dz};
        attachments.push_back(generateCherryBranch(chunk, random, treeHeight, originX, originY, originZ, trunkStateId,
                                                   sidewaysStateId, opposite, secondBranchOffset,
                                                   secondBranchOffset < trunkHeight - 1));
    }
    return attachments;
}

FoliageAttachment TrunkPlacer::generateCherryBranch(Chunk& chunk, random::RandomSource& random,
                                                    const std::int32_t treeHeight, const std::int32_t originX,
                                                    const std::int32_t originY, const std::int32_t originZ,
                                                    const std::int32_t trunkStateId, const std::int32_t sidewaysStateId,
                                                    const HorizontalDirection branchDirection,
                                                    const std::int32_t offsetFromOrigin,
                                                    const bool middleContinuesUpwards) const {
    std::int32_t logX = originX;
    std::int32_t logY = originY + offsetFromOrigin;
    std::int32_t logZ = originZ;
    const std::int32_t branchEndOffset = treeHeight - 1 + branchEndOffsetFromTop_.sample(random);
    const bool extendAway = middleContinuesUpwards || branchEndOffset < offsetFromOrigin;
    const std::int32_t distanceToTrunk = branchHorizontalLength_.sample(random) + (extendAway ? 1 : 0);
    const std::int32_t endX = originX + branchDirection.dx * distanceToTrunk;
    const std::int32_t endY = originY + branchEndOffset;
    const std::int32_t endZ = originZ + branchDirection.dz * distanceToTrunk;
    for (std::int32_t step = 0; step < (extendAway ? 2 : 1); ++step) {
        logX += branchDirection.dx;
        logZ += branchDirection.dz;
        placeLog(chunk, logX, logY, logZ, sidewaysStateId);
    }
    const std::int32_t verticalDirection = endY > logY ? 1 : -1;
    while (true) {
        const std::int32_t distance = std::abs(logX - endX) + std::abs(logY - endY) + std::abs(logZ - endZ);
        if (distance == 0) {
            return FoliageAttachment{.x = endX, .y = endY + 1, .z = endZ};
        }
        const bool growVertically =
            random.nextFloat() < static_cast<float>(std::abs(endY - logY)) / static_cast<float>(distance);
        if (growVertically) {
            logY += verticalDirection;
        } else {
            logX += branchDirection.dx;
            logZ += branchDirection.dz;
        }
        placeLog(chunk, logX, logY, logZ, growVertically ? trunkStateId : sidewaysStateId);
    }
}

} // namespace basalt::server::world::terrain::decoration::tree

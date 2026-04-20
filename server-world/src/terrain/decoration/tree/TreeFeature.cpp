#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/JavaHashSetOrder.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeatureDecorators.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

constexpr std::int32_t kMaxLeafDistance = 7;

constexpr std::array<std::array<std::int32_t, 3>, 6> kBlockNeighbours{
    std::array<std::int32_t, 3>{0, -1, 0}, std::array<std::int32_t, 3>{0, 1, 0},  std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1},  std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}};

enum class PlacementPhase : std::uint8_t { Root, Log, Leaf, Decoration };

class TreePlacementRecorder {
public:
    explicit TreePlacementRecorder(const Chunk& chunk) : chunk_(&chunk) {}

    void setPhase(PlacementPhase phase) noexcept { phase_ = phase; }

    void record(std::int32_t x, std::int32_t y, std::int32_t z) {
        const BlockPos position{.x = chunk_->chunkX() * 16 + x, .y = y, .z = chunk_->chunkZ() * 16 + z};
        switch (phase_) {
        case PlacementPhase::Root:
            roots_.add(position);
            return;
        case PlacementPhase::Log:
            logs_.add(position);
            return;
        case PlacementPhase::Leaf:
            leaves_.add(position);
            return;
        case PlacementPhase::Decoration:
            decorations_.add(position);
            return;
        }
    }

    [[nodiscard]] TreeDecoratorContext context() const {
        return TreeDecoratorContext{.logs = sorted(logs_),
                                    .leaves = sorted(leaves_),
                                    .roots = sorted(roots_),
                                    .decorations = sorted(decorations_)};
    }

private:
    [[nodiscard]] static std::vector<BlockPos> sorted(const JavaHashSetOrder<BlockPos>& positions) {
        std::vector<BlockPos> ordered = positions.inOrder();
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const BlockPos& a, const BlockPos& b) { return a.y < b.y; });
        return ordered;
    }

    const Chunk* chunk_;
    PlacementPhase phase_ = PlacementPhase::Root;
    JavaHashSetOrder<BlockPos> logs_;
    JavaHashSetOrder<BlockPos> leaves_;
    JavaHashSetOrder<BlockPos> roots_;
    JavaHashSetOrder<BlockPos> decorations_;
};

/// parity: `TreeFeature.getMaxFreeTreeHeight`.
[[nodiscard]] std::int32_t maxFreeTreeHeight(const Chunk& chunk, std::int32_t maxTreeHeight, std::int32_t originX,
                                             std::int32_t originY, std::int32_t originZ,
                                             const TreeConfiguration& config) {
    for (std::int32_t y = 0; y <= maxTreeHeight + 1; ++y) {
        const std::int32_t radius = config.minimumSize.sizeAtHeight(maxTreeHeight, y);
        for (std::int32_t x = -radius; x <= radius; ++x) {
            for (std::int32_t z = -radius; z <= radius; ++z) {
                const std::int32_t stateId = chunk.decorationBlockStateId(originX + x, originY + y, originZ + z);
                const bool blockedByTreeShape =
                    !config.trunkPlacer.isFree(chunk, originX + x, originY + y, originZ + z);
                const bool blockedByVine = !config.ignoreVines && registry::blockStateFamilies::isVineState(stateId);
                if (blockedByTreeShape || blockedByVine) {
                    return y - 2;
                }
            }
        }
    }
    return maxTreeHeight;
}

/// parity: `LeavesBlock.getOptionalDistanceAt` answers 0 for `PREVENTS_NEARBY_LEAF_DECAY` (`#minecraft:logs`).
[[nodiscard]] std::optional<std::int32_t> optionalDistanceAt(std::int32_t stateId) {
    if (treeBlockRules::isLogState(stateId)) {
        return 0;
    }
    return registry::blockStateFamilies::leafDistance(stateId);
}

/// parity: `TreeFeature.updateLeaves`.
void updateLeaves(Chunk& chunk, const TreeDecoratorContext& context) {
    std::vector<BlockPos> boundsPositions = context.roots;
    boundsPositions.insert(boundsPositions.end(), context.logs.begin(), context.logs.end());
    boundsPositions.insert(boundsPositions.end(), context.leaves.begin(), context.leaves.end());
    boundsPositions.insert(boundsPositions.end(), context.decorations.begin(), context.decorations.end());
    if (context.logs.empty() || boundsPositions.empty()) {
        return;
    }
    std::int32_t minX = boundsPositions.front().x;
    std::int32_t maxX = minX;
    std::int32_t minY = boundsPositions.front().y;
    std::int32_t maxY = minY;
    std::int32_t minZ = boundsPositions.front().z;
    std::int32_t maxZ = minZ;
    for (const BlockPos& position : boundsPositions) {
        minX = std::min(minX, position.x);
        maxX = std::max(maxX, position.x);
        minY = std::min(minY, position.y);
        maxY = std::max(maxY, position.y);
        minZ = std::min(minZ, position.z);
        maxZ = std::max(maxZ, position.z);
    }
    const auto isInside = [&](const BlockPos& pos) {
        return pos.x >= minX && pos.x <= maxX && pos.y >= minY && pos.y <= maxY && pos.z >= minZ && pos.z <= maxZ;
    };

    // parity: leaf buckets are `Sets.newHashSet()`.
    std::array<JavaHashSetOrder<BlockPos>, kMaxLeafDistance> toCheck;
    for (const BlockPos& log : context.logs) {
        toCheck[0].add(log);
    }
    std::set<BlockPos> filled{context.roots.begin(), context.roots.end()};
    filled.insert(context.decorations.begin(), context.decorations.end());

    std::int32_t smallestDistance = 0;
    while (smallestDistance < kMaxLeafDistance) {
        while (smallestDistance < kMaxLeafDistance && toCheck[static_cast<std::size_t>(smallestDistance)].empty()) {
            smallestDistance++;
        }
        if (smallestDistance >= kMaxLeafDistance) {
            return;
        }
        const BlockPos pos = toCheck[static_cast<std::size_t>(smallestDistance)].takeFirst();
        if (!isInside(pos)) {
            continue;
        }

        // parity: the jar sets the distance then fills the shape, so a twice-reached leaf is rewritten, last wins.
        if (smallestDistance != 0) {
            const std::int32_t currentStateId = chunk.decorationBlockStateId(pos.x, pos.y, pos.z);
            const std::optional<std::int32_t> updatedStateId =
                registry::blockStateFamilies::withLeafDistance(currentStateId, smallestDistance);
            if (updatedStateId.has_value()) {
                chunk.setDecorationBlockStateId(pos.x, pos.y, pos.z, *updatedStateId);
            }
        }
        filled.insert(pos);

        for (const std::array<std::int32_t, 3>& offset : kBlockNeighbours) {
            const BlockPos neighbor{.x = pos.x + offset[0], .y = pos.y + offset[1], .z = pos.z + offset[2]};
            if (!isInside(neighbor) || filled.contains(neighbor)) {
                continue;
            }
            const std::optional<std::int32_t> currentDistance =
                optionalDistanceAt(chunk.decorationBlockStateId(neighbor.x, neighbor.y, neighbor.z));
            if (!currentDistance.has_value()) {
                continue;
            }
            const std::int32_t newDistance = std::min(*currentDistance, smallestDistance + 1);
            if (newDistance < kMaxLeafDistance) {
                toCheck[static_cast<std::size_t>(newDistance)].add(neighbor);
                smallestDistance = std::min(smallestDistance, newDistance);
            }
        }
    }
}

bool generateTree(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                  std::int32_t originZ, std::int32_t trunkOriginY, std::int32_t clippedTreeHeight,
                  std::int32_t foliageHeight, std::int32_t leafRadius, const TreeConfiguration& config,
                  TreePlacementRecorder& recorder) {
    recorder.setPhase(PlacementPhase::Root);
    if (config.rootPlacer.has_value() &&
        !config.rootPlacer->placeRoots(chunk, random, originX, originY, originZ, trunkOriginY)) {
        return false;
    }
    recorder.setPhase(PlacementPhase::Log);
    const std::vector<FoliageAttachment> attachments = config.trunkPlacer.placeTrunk(
        chunk, random, clippedTreeHeight, originX, trunkOriginY, originZ, config.trunkStateId);
    FoliagePositions foliagePositions;
    recorder.setPhase(PlacementPhase::Leaf);
    for (const FoliageAttachment& attachment : attachments) {
        const std::int32_t offset = config.foliagePlacer.offsetOf(random);
        config.foliagePlacer.createFoliage(chunk, random, config.foliageStateId, attachment, foliageHeight, leafRadius,
                                           offset, foliagePositions, config.trunkStateId);
    }
    return true;
}

bool placeConfigured(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                     std::int32_t originZ, const TreeConfiguration& config, random::RandomSource& mossyCarpetRandom) {
    const std::int32_t treeHeight = config.trunkPlacer.treeHeight(random);
    const std::int32_t foliageHeight = config.foliagePlacer.foliageHeight(random, treeHeight);
    const std::int32_t trunkHeight = treeHeight - foliageHeight;
    const std::int32_t leafRadius = config.foliagePlacer.foliageRadius(random, trunkHeight);
    // parity: the trunk origin rises with a root placer, sampled after the foliage radius; both feed the bounds check.
    const std::int32_t trunkOriginY =
        config.rootPlacer.has_value() ? config.rootPlacer->trunkOriginY(random, originY) : originY;
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t minY = std::min(originY, trunkOriginY);
    const std::int32_t maxY = std::max(originY, trunkOriginY) + treeHeight + 1;
    if (minY < bounds.minY() + 1 || maxY > bounds.maxY() + 1) {
        return false;
    }

    const std::int32_t clippedTreeHeight = maxFreeTreeHeight(chunk, treeHeight, originX, trunkOriginY, originZ, config);
    const std::optional<std::int32_t> minClippedHeight = config.minimumSize.minClippedHeight();
    if (clippedTreeHeight < treeHeight && (!minClippedHeight.has_value() || clippedTreeHeight < *minClippedHeight)) {
        return false;
    }

    TreePlacementRecorder recorder{chunk};
    bool generated = false;
    {
        const Chunk::BlockWriteObserverScope observing{chunk, [&recorder](std::int32_t x, std::int32_t y,
                                                                          std::int32_t z, std::int32_t,
                                                                          std::int32_t) { recorder.record(x, y, z); }};
        generated = generateTree(chunk, random, originX, originY, originZ, trunkOriginY, clippedTreeHeight,
                                 foliageHeight, leafRadius, config, recorder);
    }
    if (!generated) {
        return false;
    }
    const TreeDecoratorContext decoratorContext = recorder.context();
    if (decoratorContext.logs.empty() && decoratorContext.leaves.empty()) {
        return false;
    }
    if (!config.decorators.empty()) {
        recorder.setPhase(PlacementPhase::Decoration);
        const Chunk::BlockWriteObserverScope observing{chunk, [&recorder](std::int32_t x, std::int32_t y,
                                                                          std::int32_t z, std::int32_t,
                                                                          std::int32_t) { recorder.record(x, y, z); }};
        for (const TreeDecoratorConfiguration& decorator : config.decorators) {
            treeDecorators::decorate(chunk, random, mossyCarpetRandom, decoratorContext, decorator);
        }
    }
    updateLeaves(chunk, recorder.context());
    return true;
}

} // namespace

bool treeFeature::place(Chunk& chunk, random::RandomSource& random, const std::int32_t originX,
                        const std::int32_t originY, const std::int32_t originZ, const TreeConfiguration& config,
                        random::RandomSource& mossyCarpetRandom) {
    const Chunk::WritePolicyScope policy{chunk, GeneratedBlockWritePolicy::Tree};
    return placeConfigured(chunk, random, originX, originY, originZ, config, mossyCarpetRandom);
}

bool treeFeature::place(Chunk& chunk, random::RandomSource& random, const std::int32_t originX,
                        const std::int32_t originY, const std::int32_t originZ, const TreeConfiguration& config) {
    return place(chunk, random, originX, originY, originZ, config, random);
}

} // namespace basalt::server::world::terrain::decoration::tree

#include <basalt/server/world/structure/JigsawAssembly.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace basalt::server::world::structure {
namespace {

/// The box a piece may still occupy: its bounds, minus everything already standing in it.
class CollisionSpace {
public:
    CollisionSpace(StructureBoundingBox bounds, std::vector<StructureBoundingBox> occupied)
        : bounds_(bounds), occupied_(std::move(occupied)) {}

    [[nodiscard]] bool canFit(const StructureBoundingBox& candidate) const {
        if (!bounds_.containsFully(candidate)) {
            return false;
        }
        return std::none_of(occupied_.begin(), occupied_.end(),
                            [&candidate](const StructureBoundingBox& box) { return box.intersects(candidate); });
    }

    void occupy(const StructureBoundingBox& box) { occupied_.push_back(box); }

private:
    StructureBoundingBox bounds_;
    std::vector<StructureBoundingBox> occupied_;
};

using CollisionSpaceRef = std::shared_ptr<CollisionSpace>;

struct QueuedPiece {
    std::size_t piece = 0;
    CollisionSpaceRef free;
    std::int32_t depth = 0;
    std::int32_t priority = 0;
    std::int32_t order = 0;
};

/// parity: `JigsawBlock.canAttach`.
[[nodiscard]] bool canAttach(const PlacedJigsaw& source, const PlacedJigsaw& target) {
    return source.front == opposite(target.front) && (source.rollable || source.top == target.top) &&
           source.target == target.name;
}

[[nodiscard]] StructurePos plus(StructurePos first, StructurePos second) {
    return StructurePos{first.x + second.x, first.y + second.y, first.z + second.z};
}

[[nodiscard]] StructurePos minus(StructurePos first, StructurePos second) {
    return StructurePos{first.x - second.x, first.y - second.y, first.z - second.z};
}

/// The height the expansion hack grows a village piece to, so its children still fit above it.
[[nodiscard]] std::int32_t expansionHeight(const JigsawWorld& world, const StructureBoundingBox& unexpandedBox,
                                           const std::vector<PlacedJigsaw>& targetJigsaws) {
    std::int32_t tallest = 0;
    for (const PlacedJigsaw& childJigsaw : targetJigsaws) {
        const StructurePos childTarget = offsetBy(childJigsaw.front, childJigsaw.pos);
        if (!unexpandedBox.isInside(childTarget)) {
            tallest = std::max(tallest, 0);
            continue;
        }
        const StructureTemplatePool* childPool = world.poolLookup(childJigsaw.pool);
        const StructureTemplatePool* childFallback =
            childPool == nullptr ? nullptr : world.poolLookup(childPool->fallback());
        const std::int32_t own = childPool == nullptr ? 0 : childPool->maxHeight(world.templateLoader);
        const std::int32_t back = childFallback == nullptr ? 0 : childFallback->maxHeight(world.templateLoader);
        tallest = std::max(tallest, std::max(own, back));
    }
    return tallest;
}

class Assembler {
public:
    Assembler(const JigsawWorld& world, const JigsawRequest& request, random::RandomSource& random)
        : world_(&world), request_(&request), random_(&random) {}

    [[nodiscard]] std::vector<PlacedStructurePiece> run();

private:
    void tryPlacingChildren(std::size_t sourceIndex, std::int32_t depth, const CollisionSpaceRef& contextFree);

    const JigsawWorld* world_;
    const JigsawRequest* request_;
    random::RandomSource* random_;
    std::vector<PlacedStructurePiece> pieces_;
    std::vector<QueuedPiece> queue_;
    std::int32_t insertionOrder_ = 1;
};

std::vector<PlacedStructurePiece> Assembler::run() {
    const StructureTemplatePool* startPool = world_->poolLookup(request_->startPool);
    if (startPool == nullptr) {
        return {};
    }
    const StructureRotation centerRotation = structureRotations::random(*random_);
    const StructurePoolElement centerElement = startPool->getRandomTemplate(*random_);
    if (centerElement.kind() == StructurePoolElement::Kind::Empty) {
        return {};
    }

    // parity: a structure that names a starting jigsaw (the ancient city's `city_anchor`) is shifted
    // so that block lands on the chosen position instead of the template's own origin. The lookup
    // shuffles, so naming one costs a draw that a structure without a name never makes.
    StructurePos anchoredPosition = request_->startPosition;
    if (!request_->startJigsawName.empty()) {
        const std::vector<PlacedJigsaw> jigsaws = centerElement.getShuffledJigsawBlocks(
            world_->templateLoader, request_->startPosition, centerRotation, *random_);
        const auto found = std::find_if(jigsaws.begin(), jigsaws.end(), [this](const PlacedJigsaw& jigsaw) {
            return jigsaw.name == request_->startJigsawName;
        });
        if (found == jigsaws.end()) {
            return {};
        }
        anchoredPosition = found->pos;
    }
    const StructurePos adjustedPosition =
        minus(request_->startPosition, minus(anchoredPosition, request_->startPosition));

    const StructureBoundingBox rawBox =
        centerElement.getBoundingBox(world_->templateLoader, adjustedPosition, centerRotation);
    const std::int32_t centerX = (rawBox.maxX + rawBox.minX) / 2;
    const std::int32_t centerZ = (rawBox.maxZ + rawBox.minZ) / 2;
    // parity: the projected floor is `position.getY() + getFirstFreeHeight(...)`, not the heightmap
    // alone. Every caller until the trail ruins started at y 0, where the two agree; the ruins start
    // at -15 and are meant to end up that far under the surface.
    const std::int32_t bottomY = request_->projectStartToHeightmap
                                     ? request_->startPosition.y + world_->heightAt(centerX, centerZ)
                                     : adjustedPosition.y;
    const std::int32_t moveY = bottomY - (rawBox.minY + centerElement.groundLevelDelta());
    const StructurePos centerPosition{adjustedPosition.x, adjustedPosition.y + moveY, adjustedPosition.z};
    const StructureBoundingBox centerBox = rawBox.move(StructurePos{0, moveY, 0});

    pieces_.push_back(PlacedStructurePiece{.element = centerElement,
                                           .position = centerPosition,
                                           .rotation = centerRotation,
                                           .boundingBox = centerBox,
                                           .groundLevelDelta = centerElement.groundLevelDelta()});
    if (request_->maxDepth <= 0) {
        return std::move(pieces_);
    }

    // parity: `JigsawPlacement`'s allowed region is the reach box intersected with the world height
    // shrunk by the dimension padding, so the floor and the ceiling bound the assembly even when the
    // vertical reach alone would not. Leaving the vertical side open lets pieces land where vanilla
    // refuses them, and every later collision test then sees a different set of neighbours.
    const bool unbounded = request_->maxVerticalDistanceFromCenter == std::numeric_limits<std::int32_t>::max();
    const std::int32_t reachMinY =
        unbounded ? std::numeric_limits<std::int32_t>::min() : bottomY - request_->maxVerticalDistanceFromCenter;
    const std::int32_t reachMaxY =
        unbounded ? std::numeric_limits<std::int32_t>::max() : bottomY + request_->maxVerticalDistanceFromCenter;
    const std::int32_t floorY = request_->worldMinY == std::numeric_limits<std::int32_t>::min()
                                    ? reachMinY
                                    : request_->worldMinY + request_->dimensionPaddingBottom;
    const std::int32_t ceilingY = request_->worldMaxY == std::numeric_limits<std::int32_t>::max()
                                      ? reachMaxY
                                      : request_->worldMaxY - request_->dimensionPaddingTop;
    const StructureBoundingBox allowedRegion{centerX - request_->maxDistanceFromCenter,
                                             std::max(reachMinY, floorY),
                                             centerZ - request_->maxDistanceFromCenter,
                                             centerX + request_->maxDistanceFromCenter,
                                             std::min(reachMaxY, ceilingY),
                                             centerZ + request_->maxDistanceFromCenter};

    const CollisionSpaceRef globalFree =
        std::make_shared<CollisionSpace>(allowedRegion, std::vector<StructureBoundingBox>{centerBox});
    queue_.push_back(QueuedPiece{
        .piece = 0, .free = globalFree, .depth = 0, .priority = std::numeric_limits<std::int32_t>::max(), .order = 0});
    while (!queue_.empty()) {
        // The highest placement priority wins, and the earliest insertion breaks the tie.
        std::size_t nextIndex = 0;
        for (std::size_t index = 1; index < queue_.size(); ++index) {
            const QueuedPiece& best = queue_[nextIndex];
            const QueuedPiece& candidate = queue_[index];
            if (candidate.priority > best.priority ||
                (candidate.priority == best.priority && candidate.order < best.order)) {
                nextIndex = index;
            }
        }
        const QueuedPiece next = queue_[nextIndex];
        queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(nextIndex));
        tryPlacingChildren(next.piece, next.depth, next.free);
    }
    return std::move(pieces_);
}

void Assembler::tryPlacingChildren(std::size_t sourceIndex, std::int32_t depth, const CollisionSpaceRef& contextFree) {
    const StructurePoolElement sourceElement = pieces_[sourceIndex].element;
    const bool sourceRigid = sourceElement.projection() == StructureProjection::Rigid;
    const StructureBoundingBox sourceBB = pieces_[sourceIndex].boundingBox;
    const std::vector<PlacedJigsaw> sourceJigsaws = sourceElement.getShuffledJigsawBlocks(
        world_->templateLoader, pieces_[sourceIndex].position, pieces_[sourceIndex].rotation, *random_);
    CollisionSpaceRef insideSourceFree;

    for (const PlacedJigsaw& sourceJigsaw : sourceJigsaws) {
        const StructurePos targetJigsawPos = offsetBy(sourceJigsaw.front, sourceJigsaw.pos);
        const std::int32_t sourceJigsawLocalY = sourceJigsaw.pos.y - sourceBB.minY;
        CollisionSpaceRef childrenFree = contextFree;
        if (sourceBB.isInside(targetJigsawPos)) {
            if (insideSourceFree == nullptr) {
                insideSourceFree = std::make_shared<CollisionSpace>(sourceBB, std::vector<StructureBoundingBox>{});
            }
            childrenFree = insideSourceFree;
        }

        // parity: `poolAliasLookup.lookup(sourceJigsaw.pool())`, applied before the pool is resolved.
        const auto aliased = request_->poolAliases.find(sourceJigsaw.pool);
        const std::string_view poolName =
            aliased == request_->poolAliases.end() ? std::string_view{sourceJigsaw.pool} : aliased->second;
        const StructureTemplatePool* targetPool = world_->poolLookup(poolName);
        if (targetPool == nullptr || targetPool->size() == 0) {
            continue;
        }
        const StructureTemplatePool* fallbackPool = world_->poolLookup(targetPool->fallback());

        std::vector<StructurePoolElement> targetPieces;
        if (depth != request_->maxDepth) {
            targetPieces = targetPool->getShuffledTemplates(*random_);
        }
        if (fallbackPool != nullptr) {
            const std::vector<StructurePoolElement> fallback = fallbackPool->getShuffledTemplates(*random_);
            targetPieces.insert(targetPieces.end(), fallback.begin(), fallback.end());
        }

        bool placed = false;
        for (const StructurePoolElement& targetElement : targetPieces) {
            if (placed || targetElement.kind() == StructurePoolElement::Kind::Empty) {
                break;
            }
            for (const StructureRotation targetRotation : structureRotations::shuffled(*random_)) {
                const std::vector<PlacedJigsaw> targetJigsaws = targetElement.getShuffledJigsawBlocks(
                    world_->templateLoader, StructurePos{0, 0, 0}, targetRotation, *random_);
                const bool targetRigid = targetElement.projection() == StructureProjection::Rigid;
                const StructureBoundingBox unexpandedBox =
                    targetElement.getBoundingBox(world_->templateLoader, StructurePos{0, 0, 0}, targetRotation);
                const std::int32_t expandTo =
                    request_->useExpansionHack && unexpandedBox.maxY - unexpandedBox.minY + 1 <= 16
                        ? expansionHeight(*world_, unexpandedBox, targetJigsaws)
                        : 0;

                for (const PlacedJigsaw& targetJigsaw : targetJigsaws) {
                    if (!canAttach(sourceJigsaw, targetJigsaw)) {
                        continue;
                    }
                    const StructurePos rawTargetBoxPos = minus(targetJigsawPos, targetJigsaw.pos);
                    const StructureBoundingBox rawTargetBB =
                        targetElement.getBoundingBox(world_->templateLoader, rawTargetBoxPos, targetRotation);
                    const std::int32_t targetJigsawLocalY = targetJigsaw.pos.y;
                    const std::int32_t deltaY = sourceJigsawLocalY - targetJigsawLocalY + dy(sourceJigsaw.front);
                    const std::int32_t targetBoxY =
                        sourceRigid && targetRigid
                            ? sourceBB.minY + deltaY
                            : world_->heightAt(sourceJigsaw.pos.x, sourceJigsaw.pos.z) - targetJigsawLocalY;
                    const std::int32_t yOffset = targetBoxY - rawTargetBB.minY;
                    StructureBoundingBox targetBB = rawTargetBB.move(StructurePos{0, yOffset, 0});
                    if (expandTo > 0) {
                        targetBB.maxY = targetBB.minY + std::max(expandTo + 1, targetBB.maxY - targetBB.minY);
                    }
                    if (!childrenFree->canFit(targetBB)) {
                        continue;
                    }
                    childrenFree->occupy(targetBB);
                    const std::int32_t sourceGroundLevelDelta = pieces_[sourceIndex].groundLevelDelta;
                    const std::int32_t targetGroundLevelDelta =
                        targetRigid ? sourceGroundLevelDelta - deltaY : targetElement.groundLevelDelta();
                    const std::int32_t junctionY =
                        sourceRigid   ? sourceBB.minY + sourceJigsawLocalY
                        : targetRigid ? targetBoxY + targetJigsawLocalY
                                      : world_->heightAt(sourceJigsaw.pos.x, sourceJigsaw.pos.z) + deltaY / 2;
                    pieces_[sourceIndex].junctions.push_back(
                        PlacedJigsawJunction{.sourceX = targetJigsawPos.x,
                                             .sourceGroundY = junctionY - sourceJigsawLocalY + sourceGroundLevelDelta,
                                             .sourceZ = targetJigsawPos.z,
                                             .deltaY = deltaY,
                                             .destinationProjection = targetElement.projection()});
                    PlacedStructurePiece targetPiece{.element = targetElement,
                                                     .position = plus(rawTargetBoxPos, StructurePos{0, yOffset, 0}),
                                                     .rotation = targetRotation,
                                                     .boundingBox = targetBB,
                                                     .groundLevelDelta = targetGroundLevelDelta};
                    targetPiece.junctions.push_back(
                        PlacedJigsawJunction{.sourceX = sourceJigsaw.pos.x,
                                             .sourceGroundY = junctionY - targetJigsawLocalY + targetGroundLevelDelta,
                                             .sourceZ = sourceJigsaw.pos.z,
                                             .deltaY = -deltaY,
                                             .destinationProjection = sourceElement.projection()});
                    pieces_.push_back(std::move(targetPiece));
                    if (depth + 1 <= request_->maxDepth) {
                        queue_.push_back(QueuedPiece{.piece = pieces_.size() - 1,
                                                     .free = childrenFree,
                                                     .depth = depth + 1,
                                                     .priority = sourceJigsaw.placementPriority,
                                                     .order = insertionOrder_++});
                    }
                    placed = true;
                    break;
                }
                if (placed) {
                    break;
                }
            }
        }
    }
}

} // namespace

std::vector<PlacedStructurePiece> assembleJigsaw(const JigsawWorld& world, const JigsawRequest& request,
                                                 random::RandomSource& random) {
    return Assembler{world, request, random}.run();
}

} // namespace basalt::server::world::structure

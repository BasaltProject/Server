#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/WalkNodeEvaluator.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <span>
#include <unordered_map>

namespace basalt::server::runtime::entity::walkNodes {
namespace {

using pathTypes::PathType;

/// parity: `PathFinder`'s 1.5 on the heuristic.
constexpr float kFudging = 1.5F;
/// parity: `WalkNodeEvaluator.DEFAULT_MOB_JUMP_HEIGHT`.
constexpr double kJumpHeight = static_cast<double>(1.125F);
/// parity: `Creaking.HomeNodeEvaluator.MAX_DISTANCE_TO_HOME_SQ`.
constexpr double kHomeDistanceSquared = 1024.0;

/// parity: `Direction.Plane.HORIZONTAL`: north, east, south, west, each the next one's anticlockwise.
constexpr std::array<std::array<std::int32_t, 2>, 4> kHorizontal{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

/// parity: `Node`, with the search's bookkeeping.
struct SearchNode {
    Position at{};
    std::int32_t heapIdx = -1;
    float g = 0.0F;
    float h = 0.0F;
    float f = 0.0F;
    SearchNode* cameFrom = nullptr;
    bool closed = false;
    float walkedDistance = 0.0F;
    float costMalus = 0.0F;
    PathType type = PathType::Blocked;

    [[nodiscard]] bool inOpenSet() const { return heapIdx >= 0; }
};

/// parity: `Node.createHash`, the key the evaluator files its nodes under.
[[nodiscard]] std::int32_t nodeHash(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::uint32_t hash =
        (static_cast<std::uint32_t>(y) & 255U) | ((static_cast<std::uint32_t>(x) & 32767U) << 8U) |
        ((static_cast<std::uint32_t>(z) & 32767U) << 24U) | (x < 0 ? 0x80000000U : 0U) | (z < 0 ? 0x8000U : 0U);
    return static_cast<std::int32_t>(hash);
}

[[nodiscard]] std::int64_t blockKey(std::int32_t x, std::int32_t y, std::int32_t z) {
    return static_cast<std::int64_t>(((static_cast<std::uint64_t>(x) & 0x3FFFFFFU) << 38U) |
                                     ((static_cast<std::uint64_t>(z) & 0x3FFFFFFU) << 12U) |
                                     (static_cast<std::uint64_t>(y) & 0xFFFU));
}

/// parity: `Node.distanceTo`, in floats.
[[nodiscard]] float distanceTo(const Position& from, const Position& to) {
    const auto xd = static_cast<float>(to[0] - from[0]);
    const auto yd = static_cast<float>(to[1] - from[1]);
    const auto zd = static_cast<float>(to[2] - from[2]);
    return static_cast<float>(std::sqrt(static_cast<double>((xd * xd) + (yd * yd) + (zd * zd))));
}

/// parity: `Node.distanceManhattan`.
[[nodiscard]] float manhattan(const Position& from, const Position& to) {
    const auto xd = static_cast<float>(std::abs(to[0] - from[0]));
    const auto yd = static_cast<float>(std::abs(to[1] - from[1]));
    const auto zd = static_cast<float>(std::abs(to[2] - from[2]));
    return xd + yd + zd;
}

/// parity: `BinaryHeap`, whose tie-breaking decides between equally good paths.
class BinaryHeap {
public:
    void insert(SearchNode* node) {
        heap_.push_back(node);
        node->heapIdx = static_cast<std::int32_t>(heap_.size() - 1);
        upHeap(node->heapIdx);
    }

    [[nodiscard]] SearchNode* pop() {
        SearchNode* popped = heap_.front();
        heap_.front() = heap_.back();
        heap_.pop_back();
        if (!heap_.empty()) {
            downHeap(0);
        }
        popped->heapIdx = -1;
        return popped;
    }

    void changeCost(SearchNode* node, float cost) {
        const float old = node->f;
        node->f = cost;
        if (cost < old) {
            upHeap(node->heapIdx);
        } else {
            downHeap(node->heapIdx);
        }
    }

    [[nodiscard]] bool empty() const { return heap_.empty(); }

private:
    void place(std::int32_t index, SearchNode* node) {
        heap_[static_cast<std::size_t>(index)] = node;
        node->heapIdx = index;
    }

    void upHeap(std::int32_t index) {
        SearchNode* node = heap_[static_cast<std::size_t>(index)];
        const float cost = node->f;
        while (index > 0) {
            const std::int32_t parentIndex = (index - 1) >> 1;
            SearchNode* parent = heap_[static_cast<std::size_t>(parentIndex)];
            if (!(cost < parent->f)) {
                break;
            }
            place(index, parent);
            index = parentIndex;
        }
        place(index, node);
    }

    void downHeap(std::int32_t index) {
        SearchNode* node = heap_[static_cast<std::size_t>(index)];
        const float cost = node->f;
        const auto size = static_cast<std::int32_t>(heap_.size());
        while (true) {
            const std::int32_t leftIndex = 1 + (index << 1);
            const std::int32_t rightIndex = leftIndex + 1;
            if (leftIndex >= size) {
                break;
            }
            SearchNode* left = heap_[static_cast<std::size_t>(leftIndex)];
            const float leftCost = left->f;
            SearchNode* right = nullptr;
            float rightCost = std::numeric_limits<float>::infinity();
            if (rightIndex < size) {
                right = heap_[static_cast<std::size_t>(rightIndex)];
                rightCost = right->f;
            }
            if (leftCost < rightCost) {
                if (!(leftCost < cost)) {
                    break;
                }
                place(index, left);
                index = leftIndex;
            } else {
                if (!(rightCost < cost)) {
                    break;
                }
                place(index, right);
                index = rightIndex;
            }
        }
        place(index, node);
    }

    std::vector<SearchNode*> heap_;
};

/// One search: `WalkNodeEvaluator` between `prepare` and `done`, with the finder around it.
class Search {
public:
    Search(world::WorldState& world, const WalkMob& mob)
        : world_(&world), mob_(&mob), blocks_(world.blockAccessor()),
          mobAt_{static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
                 static_cast<std::int32_t>(std::floor(mob.z))},
          entityWidth_(static_cast<std::int32_t>(std::floor(mob.width + 1.0F))),
          entityHeight_(static_cast<std::int32_t>(std::floor(mob.height + 1.0F))) {
        const double half = static_cast<double>(mob.width / 2.0F);
        box_ = Aabb{mob.x - half, mob.y, mob.z - half, mob.x + half, mob.y + static_cast<double>(mob.height),
                    mob.z + half};
    }

    [[nodiscard]] Path run(const Position& target, float maxPathLength, std::int32_t reachRange,
                           std::int32_t maxVisitedNodes);

private:
    [[nodiscard]] float malus(PathType type) const { return pathTypes::malusOf(mob_->maluses, type); }

    [[nodiscard]] std::optional<std::int32_t> stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return world_->blockState(x, y, z);
    }

    [[nodiscard]] bool isAir(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const std::optional<std::int32_t> state = stateAt(x, y, z);
        return !state.has_value() || registry::blocks::isAir(*state);
    }

    /// parity: `FluidState.is(FluidTags.ENTITY_FLOATABLE)`, which is water.
    [[nodiscard]] bool floatable(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const std::optional<std::int32_t> state = stateAt(x, y, z);
        return state.has_value() && registry::blockSupport::holdsWater(*state);
    }

    SearchNode* node(std::int32_t x, std::int32_t y, std::int32_t z);
    [[nodiscard]] PathType pathTypeStatic(std::int32_t x, std::int32_t y, std::int32_t z) const;
    [[nodiscard]] PathType pathType(std::int32_t x, std::int32_t y, std::int32_t z) const;
    [[nodiscard]] PathType pathTypeOfMob(std::int32_t x, std::int32_t y, std::int32_t z) const;
    [[nodiscard]] PathType cachedPathType(std::int32_t x, std::int32_t y, std::int32_t z);
    [[nodiscard]] double floorLevel(std::int32_t x, std::int32_t y, std::int32_t z) const;
    [[nodiscard]] bool hasCollisions(const Aabb& box);
    [[nodiscard]] bool canReachWithoutCollision(const SearchNode& to);
    [[nodiscard]] bool canStartAt(std::int32_t x, std::int32_t y, std::int32_t z);
    SearchNode* startNode(std::int32_t x, std::int32_t y, std::int32_t z);
    SearchNode* start();
    SearchNode* updateCostToMax(std::int32_t x, std::int32_t y, std::int32_t z, PathType type, float cost);
    SearchNode* blockedNode(std::int32_t x, std::int32_t y, std::int32_t z);
    SearchNode* acceptedNode(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t jumpSize, double nodeHeight,
                             std::size_t direction, PathType current);
    SearchNode* jumpOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t jumpSize, double nodeHeight,
                       std::size_t direction, PathType current);
    SearchNode* firstNonWaterBelow(std::int32_t x, std::int32_t y, std::int32_t z, SearchNode* best);
    SearchNode* firstGroundBelow(std::int32_t x, std::int32_t y, std::int32_t z);
    [[nodiscard]] bool diagonalValid(const SearchNode& at, const SearchNode* ew, const SearchNode* ns) const;
    std::size_t neighbours(std::array<SearchNode*, 32>& found, const SearchNode& at);

    world::WorldState* world_;
    const WalkMob* mob_;
    entityCollision::BlockAt blocks_;
    Position mobAt_;
    std::int32_t entityWidth_;
    std::int32_t entityHeight_;
    Aabb box_;
    std::deque<SearchNode> pool_;
    std::unordered_map<std::int32_t, SearchNode*> nodes_;
    std::unordered_map<std::int64_t, PathType> typeCache_;
    std::map<std::array<double, 6>, bool> collisionCache_;
};

SearchNode* Search::node(std::int32_t x, std::int32_t y, std::int32_t z) {
    const auto [found, inserted] = nodes_.try_emplace(nodeHash(x, y, z), nullptr);
    if (inserted) {
        found->second = &pool_.emplace_back(SearchNode{.at = {x, y, z}});
    }
    return found->second;
}

PathType Search::pathTypeStatic(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const PathType type = pathTypes::fromState(*world_, x, y, z);
    if (type != PathType::Open || y < mob_->minY + 1) {
        return type;
    }
    switch (pathTypes::fromState(*world_, x, y - 1, z)) {
    case PathType::Open:
    case PathType::Water:
    case PathType::Lava:
    case PathType::Walkable:
        return PathType::Open;
    case PathType::Fire:
        return PathType::Fire;
    case PathType::Damaging:
        return PathType::Damaging;
    case PathType::StickyHoney:
        return PathType::StickyHoney;
    case PathType::PowderSnow:
        return PathType::OnTopOfPowderSnow;
    case PathType::DamageCautious:
        return PathType::DamageCautious;
    case PathType::Trapdoor:
        return PathType::OnTopOfTrapdoor;
    default:
        return pathTypes::checkNeighbours(*world_, x, y, z, PathType::Walkable);
    }
}

PathType Search::pathType(std::int32_t x, std::int32_t y, std::int32_t z) const {
    // parity: `Creaking.HomeNodeEvaluator.getPathType`.
    if (mob_->home.has_value()) {
        const Position& home = *mob_->home;
        const auto distanceSquared = [&home](const Position& to) {
            const double dx = static_cast<double>(home[0]) - to[0];
            const double dy = static_cast<double>(home[1]) - to[1];
            const double dz = static_cast<double>(home[2]) - to[2];
            return (dx * dx) + (dy * dy) + (dz * dz);
        };
        const double homeDistance = distanceSquared(Position{x, y, z});
        if (homeDistance > kHomeDistanceSquared && homeDistance >= distanceSquared(mobAt_)) {
            return PathType::Blocked;
        }
    }
    return pathTypeStatic(x, y, z);
}

PathType Search::pathTypeOfMob(std::int32_t x, std::int32_t y, std::int32_t z) const {
    // parity: `getPathTypeWithinMobBB`, an `EnumSet` walked in id order.
    std::array<bool, pathTypes::kCount> seen{};
    std::size_t distinct = 0;
    for (std::int32_t dx = 0; dx < entityWidth_; ++dx) {
        for (std::int32_t dy = 0; dy < entityHeight_; ++dy) {
            for (std::int32_t dz = 0; dz < entityWidth_; ++dz) {
                PathType type = pathType(x + dx, y + dy, z + dz);
                if (type == PathType::DoorWoodClosed && mob_->canOpenDoors && mob_->canPassDoors) {
                    type = PathType::WalkableDoor;
                }
                if (type == PathType::DoorOpen && !mob_->canPassDoors) {
                    type = PathType::Blocked;
                }
                if (type == PathType::Rail && pathType(mobAt_[0], mobAt_[1], mobAt_[2]) != PathType::Rail &&
                    pathType(mobAt_[0], mobAt_[1] - 1, mobAt_[2]) != PathType::Rail) {
                    type = PathType::UnpassableRail;
                }
                const auto index = static_cast<std::size_t>(type);
                distinct += seen[index] ? 0 : 1;
                seen[index] = true;
            }
        }
    }
    const auto has = [&seen](PathType type) { return seen[static_cast<std::size_t>(type)]; };
    if (distinct == 1) {
        for (std::size_t index = 0; index < pathTypes::kCount; ++index) {
            if (seen[index]) {
                return static_cast<PathType>(index);
            }
        }
    }
    if (has(PathType::Fence)) {
        return PathType::Fence;
    }
    if (has(PathType::UnpassableRail)) {
        return PathType::UnpassableRail;
    }
    PathType highest = PathType::Blocked;
    float highestMalus = malus(highest);
    for (std::size_t index = 0; index < pathTypes::kCount; ++index) {
        if (!seen[index]) {
            continue;
        }
        const auto type = static_cast<PathType>(index);
        const float cost = malus(type);
        if (cost < 0.0F) {
            return type;
        }
        if (cost >= highestMalus) {
            highestMalus = cost;
            highest = type;
        }
    }
    const PathType own = pathType(x, y, z);
    if (entityWidth_ > 1) {
        const bool cheaper = malus(own) < highestMalus && malus(PathType::BigMobsCloseToDanger) < highestMalus;
        return cheaper ? PathType::BigMobsCloseToDanger : highest;
    }
    return own == PathType::Open && highest != PathType::Open && highestMalus == 0.0F ? PathType::Open : highest;
}

PathType Search::cachedPathType(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int64_t key = blockKey(x, y, z);
    if (const auto found = typeCache_.find(key); found != typeCache_.end()) {
        return found->second;
    }
    const PathType type = pathTypeOfMob(x, y, z);
    typeCache_.emplace(key, type);
    return type;
}

double Search::floorLevel(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (mob_->canFloat && floatable(x, y, z)) {
        return static_cast<double>(y) + 0.5;
    }
    return walkNodes::floorLevel(*world_, x, y, z);
}

bool Search::hasCollisions(const Aabb& box) {
    // parity: `Level.noCollision(mob, box)`, inverted and remembered per box.
    const std::array<double, 6> key{box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
    if (const auto found = collisionCache_.find(key); found != collisionCache_.end()) {
        return found->second;
    }
    const bool collides = entityCollision::collides(blocks_, box);
    collisionCache_.emplace(key, collides);
    return collides;
}

bool Search::canReachWithoutCollision(const SearchNode& to) {
    // parity: the sweep starts from where the mob stands, not from the node it would leave.
    Aabb box = box_;
    const double sizeX = box.maxX - box.minX;
    const double sizeY = box.maxY - box.minY;
    const double sizeZ = box.maxZ - box.minZ;
    double dx = to.at[0] - mob_->x + (sizeX / 2.0);
    double dy = to.at[1] - mob_->y + (sizeY / 2.0);
    double dz = to.at[2] - mob_->z + (sizeZ / 2.0);
    const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    const auto steps = static_cast<std::int32_t>(std::ceil(length / ((sizeX + sizeY + sizeZ) / 3.0)));
    const auto scale = static_cast<double>(1.0F / static_cast<float>(steps));
    dx *= scale;
    dy *= scale;
    dz *= scale;
    for (std::int32_t step = 1; step <= steps; ++step) {
        box = box.move(dx, dy, dz);
        if (hasCollisions(box)) {
            return false;
        }
    }
    return true;
}

bool Search::canStartAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const PathType type = cachedPathType(x, y, z);
    return type != PathType::Open && malus(type) >= 0.0F;
}

SearchNode* Search::startNode(std::int32_t x, std::int32_t y, std::int32_t z) {
    SearchNode* first = node(x, y, z);
    first->type = cachedPathType(x, y, z);
    first->costMalus = malus(first->type);
    return first;
}

SearchNode* Search::start() {
    // parity: `WalkNodeEvaluator.getStart`, for a mob that cannot stand on a fluid.
    const auto blockX = static_cast<std::int32_t>(std::floor(mob_->x));
    const auto blockZ = static_cast<std::int32_t>(std::floor(mob_->z));
    auto startY = static_cast<std::int32_t>(std::floor(mob_->y));
    if (mob_->canFloat && mob_->inFloatableFluid) {
        while (floatable(blockX, startY, blockZ)) {
            ++startY;
        }
        --startY;
    } else if (mob_->onGround) {
        startY = static_cast<std::int32_t>(std::floor(mob_->y + 0.5));
    } else {
        auto probeY = static_cast<std::int32_t>(std::floor(mob_->y + 1.0));
        while (probeY > mob_->minY) {
            startY = probeY;
            --probeY;
            const std::optional<std::int32_t> below = stateAt(blockX, probeY, blockZ);
            if (below.has_value() && !registry::blocks::isAir(*below) && !pathTypes::isPathfindableOnLand(*below)) {
                break;
            }
        }
    }
    if (!canStartAt(blockX, startY, blockZ)) {
        const std::array<std::array<double, 2>, 4> corners{
            {{box_.minX, box_.minZ}, {box_.minX, box_.maxZ}, {box_.maxX, box_.minZ}, {box_.maxX, box_.maxZ}}};
        for (const std::array<double, 2>& corner : corners) {
            const auto cornerX = static_cast<std::int32_t>(std::floor(corner[0]));
            const auto cornerZ = static_cast<std::int32_t>(std::floor(corner[1]));
            if (canStartAt(cornerX, startY, cornerZ)) {
                return startNode(cornerX, startY, cornerZ);
            }
        }
    }
    return startNode(blockX, startY, blockZ);
}

SearchNode* Search::updateCostToMax(std::int32_t x, std::int32_t y, std::int32_t z, PathType type, float cost) {
    SearchNode* found = node(x, y, z);
    found->type = type;
    found->costMalus = std::max(found->costMalus, cost);
    return found;
}

SearchNode* Search::blockedNode(std::int32_t x, std::int32_t y, std::int32_t z) {
    SearchNode* found = node(x, y, z);
    found->type = PathType::Blocked;
    found->costMalus = -1.0F;
    return found;
}

/// parity: `doesBlockHavePartialCollision`.
[[nodiscard]] bool partialCollision(PathType type) {
    return type == PathType::Fence || type == PathType::DoorWoodClosed || type == PathType::DoorIronClosed;
}

SearchNode* Search::acceptedNode(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t jumpSize,
                                 double nodeHeight, std::size_t direction, PathType current) {
    if (floorLevel(x, y, z) - nodeHeight > std::max(kJumpHeight, static_cast<double>(mob_->maxUpStep))) {
        return nullptr;
    }
    SearchNode* best = nullptr;
    const PathType type = cachedPathType(x, y, z);
    const float cost = malus(type);
    if (cost >= 0.0F) {
        best = updateCostToMax(x, y, z, type, cost);
    }
    if (partialCollision(current) && best != nullptr && best->costMalus >= 0.0F && !canReachWithoutCollision(*best)) {
        best = nullptr;
    }
    if (type == PathType::Walkable) {
        return best;
    }
    if ((best == nullptr || best->costMalus < 0.0F) && jumpSize > 0 &&
        (type != PathType::Fence || mob_->canWalkOverFences) && type != PathType::UnpassableRail &&
        type != PathType::Trapdoor && type != PathType::PowderSnow) {
        return jumpOn(x, y, z, jumpSize, nodeHeight, direction, current);
    }
    if (type == PathType::Water && !mob_->canFloat) {
        return firstNonWaterBelow(x, y, z, best);
    }
    if (type == PathType::Open) {
        return firstGroundBelow(x, y, z);
    }
    if (partialCollision(type) && best == nullptr) {
        // parity: `getClosedNode`, shut for the rest of the search at the type's own malus.
        SearchNode* closed = node(x, y, z);
        closed->closed = true;
        closed->type = type;
        closed->costMalus = pathTypes::malusOf(pathTypes::defaults(), type);
        return closed;
    }
    return best;
}

SearchNode* Search::jumpOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t jumpSize, double nodeHeight,
                           std::size_t direction, PathType current) {
    SearchNode* above = acceptedNode(x, y + 1, z, jumpSize - 1, nodeHeight, direction, current);
    if (above == nullptr || mob_->width >= 1.0F ||
        (above->type != PathType::Open && above->type != PathType::Walkable)) {
        return above;
    }
    const std::int32_t backX = x - kHorizontal[direction][0];
    const std::int32_t backZ = z - kHorizontal[direction][1];
    const double centerX = static_cast<double>(backX) + 0.5;
    const double centerZ = static_cast<double>(backZ) + 0.5;
    const double halfWidth = static_cast<double>(mob_->width) / 2.0;
    const double bottom = floorLevel(backX, y + 1, backZ) + 0.001;
    const double top = static_cast<double>(mob_->height) + floorLevel(above->at[0], above->at[1], above->at[2]) - 0.002;
    const Aabb grow{centerX - halfWidth, std::min(bottom, top), centerZ - halfWidth,
                    centerX + halfWidth, std::max(bottom, top), centerZ + halfWidth};
    return hasCollisions(grow) ? nullptr : above;
}

SearchNode* Search::firstNonWaterBelow(std::int32_t x, std::int32_t y, std::int32_t z, SearchNode* best) {
    for (--y; y > mob_->minY; --y) {
        const PathType type = cachedPathType(x, y, z);
        if (type != PathType::Water) {
            return best;
        }
        best = updateCostToMax(x, y, z, type, malus(type));
    }
    return best;
}

SearchNode* Search::firstGroundBelow(std::int32_t x, std::int32_t y, std::int32_t z) {
    for (std::int32_t below = y - 1; below >= mob_->minY; --below) {
        if (y - below > mob_->maxFallDistance) {
            return blockedNode(x, below, z);
        }
        const PathType type = cachedPathType(x, below, z);
        const float cost = malus(type);
        if (type != PathType::Open) {
            return cost >= 0.0F ? updateCostToMax(x, below, z, type, cost) : blockedNode(x, below, z);
        }
    }
    return blockedNode(x, y, z);
}

bool Search::diagonalValid(const SearchNode& at, const SearchNode* ew, const SearchNode* ns) const {
    if (ns == nullptr || ew == nullptr || ns->at[1] > at.at[1] || ew->at[1] > at.at[1]) {
        return false;
    }
    if (ew->type == PathType::WalkableDoor || ns->type == PathType::WalkableDoor) {
        return false;
    }
    if (mob_->width > 1.0F && (ew->costMalus > 0.0F || ns->costMalus > 0.0F)) {
        return false;
    }
    const bool betweenPosts =
        ns->type == PathType::Fence && ew->type == PathType::Fence && static_cast<double>(mob_->width) < 0.5;
    return (ns->at[1] < at.at[1] || ns->costMalus >= 0.0F || betweenPosts) &&
           (ew->at[1] < at.at[1] || ew->costMalus >= 0.0F || betweenPosts);
}

std::size_t Search::neighbours(std::array<SearchNode*, 32>& found, const SearchNode& at) {
    std::size_t count = 0;
    std::int32_t jumpSize = 0;
    const PathType above = cachedPathType(at.at[0], at.at[1] + 1, at.at[2]);
    const PathType current = cachedPathType(at.at[0], at.at[1], at.at[2]);
    if (malus(above) >= 0.0F && current != PathType::StickyHoney) {
        jumpSize = static_cast<std::int32_t>(std::floor(std::max(1.0F, mob_->maxUpStep)));
    }
    const double height = floorLevel(at.at[0], at.at[1], at.at[2]);
    std::array<SearchNode*, 4> sides{};
    for (std::size_t direction = 0; direction < kHorizontal.size(); ++direction) {
        SearchNode* side = acceptedNode(at.at[0] + kHorizontal[direction][0], at.at[1],
                                        at.at[2] + kHorizontal[direction][1], jumpSize, height, direction, current);
        sides[direction] = side;
        // parity: `isNeighborValid`.
        if (side != nullptr && !side->closed && (side->costMalus >= 0.0F || at.costMalus < 0.0F)) {
            found[count++] = side;
        }
    }
    for (std::size_t direction = 0; direction < kHorizontal.size(); ++direction) {
        const std::size_t clockwise = (direction + 1) % kHorizontal.size();
        if (!diagonalValid(at, sides[direction], sides[clockwise])) {
            continue;
        }
        SearchNode* diagonal = acceptedNode(at.at[0] + kHorizontal[direction][0] + kHorizontal[clockwise][0], at.at[1],
                                            at.at[2] + kHorizontal[direction][1] + kHorizontal[clockwise][1], jumpSize,
                                            height, direction, current);
        if (diagonal != nullptr && !diagonal->closed && diagonal->type != PathType::WalkableDoor &&
            diagonal->costMalus >= 0.0F) {
            found[count++] = diagonal;
        }
    }
    return count;
}

Path Search::run(const Position& target, float maxPathLength, std::int32_t reachRange, std::int32_t maxVisitedNodes) {
    SearchNode* from = start();
    float bestHeuristic = std::numeric_limits<float>::max();
    SearchNode* bestNode = nullptr;
    bool reached = false;
    // parity: `getBestH`, which also keeps the target's nearest node.
    const auto bestH = [&](SearchNode* candidate) {
        const float h = distanceTo(candidate->at, target);
        if (h < bestHeuristic) {
            bestHeuristic = h;
            bestNode = candidate;
        }
        return h;
    };
    from->g = 0.0F;
    from->h = bestH(from);
    from->f = from->h;
    BinaryHeap open;
    open.insert(from);
    std::int32_t count = 0;
    std::array<SearchNode*, 32> found{};
    while (!open.empty()) {
        if (++count >= maxVisitedNodes) {
            break;
        }
        SearchNode* current = open.pop();
        current->closed = true;
        if (manhattan(current->at, target) <= static_cast<float>(reachRange)) {
            reached = true;
            break;
        }
        if (distanceTo(current->at, from->at) >= maxPathLength) {
            continue;
        }
        const std::size_t neighbourCount = neighbours(found, *current);
        for (std::size_t index = 0; index < neighbourCount; ++index) {
            SearchNode* next = found[index];
            const float distance = distanceTo(current->at, next->at);
            next->walkedDistance = current->walkedDistance + distance;
            const float tentative = current->g + distance + next->costMalus;
            if (next->walkedDistance < maxPathLength && (!next->inOpenSet() || tentative < next->g)) {
                next->cameFrom = current;
                next->g = tentative;
                next->h = bestH(next) * kFudging;
                if (next->inOpenSet()) {
                    open.changeCost(next, next->g + next->h);
                } else {
                    next->f = next->g + next->h;
                    open.insert(next);
                }
            }
        }
    }
    Path path{.target = target, .reached = reached};
    for (const SearchNode* at = bestNode; at != nullptr; at = at->cameFrom) {
        path.nodes.push_back(Node{.at = at->at, .type = at->type});
    }
    std::ranges::reverse(path.nodes);
    return path;
}

} // namespace

double floorLevel(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    double top = 0.0;
    if (below.has_value()) {
        const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(*below);
        for (std::size_t at = 0; at + 5 < boxes.size(); at += registry::blockShapes::kCorners) {
            top = at == 0 ? boxes[at + 4] : std::max(top, static_cast<double>(boxes[at + 4]));
        }
        top /= registry::blockShapes::kUnits;
    }
    return static_cast<double>(y - 1) + top;
}

Path findPath(world::WorldState& world, const WalkMob& mob, const Position& target, float maxPathLength,
              std::int32_t reachRange, std::int32_t maxVisitedNodes) {
    Search search{world, mob};
    return search.run(target, maxPathLength, reachRange, maxVisitedNodes);
}

} // namespace basalt::server::runtime::entity::walkNodes

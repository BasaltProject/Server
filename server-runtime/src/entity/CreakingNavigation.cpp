#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/CreakingNavigation.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WalkNodeEvaluator.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity::creakingNavigation {
namespace {

using Block = std::array<std::int32_t, 3>;
using Vec = std::array<double, 3>;
using pathTypes::PathType;

constexpr std::string_view kFollowRange = "minecraft:follow_range";
constexpr std::string_view kStepHeight = "minecraft:step_height";
/// parity: `PathNavigation.requiredPathLength`'s sixteen.
constexpr float kRequiredPathLength = 16.0F;
/// The overworld's floor, for a creaking whose chunk is somehow not loaded.
constexpr std::int32_t kFallbackMinY = -64;

[[nodiscard]] std::int32_t floorOf(double value) { return static_cast<std::int32_t>(std::floor(value)); }

[[nodiscard]] bool isAirAt(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return !state.has_value() || registry::blocks::isAir(*state);
}

[[nodiscard]] bool isSolidAt(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && registry::blockShapes::isSolidState(*state);
}

[[nodiscard]] bool floatableAt(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && registry::blockSupport::holdsWater(*state);
}

/// parity: `GroundPathNavigation.canUpdatePath`.
[[nodiscard]] bool canUpdatePath(world::WorldState& world, const ServerEntity& creaking) {
    return creaking.onGround || ai::inWater(world, creaking) || world::fluids::touchesLava(world, creaking) ||
           creaking.vehicleId.has_value();
}

/// parity: `GroundPathNavigation.getTempMobPos`, the feet or, afloat, the surface above them.
[[nodiscard]] Vec tempPosition(world::WorldState& world, const ServerEntity& creaking) {
    const std::int32_t blockY = floorOf(creaking.y);
    if (!ai::inWater(world, creaking)) {
        return {creaking.x, static_cast<double>(floorOf(creaking.y + 0.5)), creaking.z};
    }
    std::int32_t surface = blockY;
    std::int32_t steps = 0;
    while (floatableAt(world, floorOf(creaking.x), surface, floorOf(creaking.z))) {
        ++surface;
        if (++steps > 16) {
            surface = blockY;
            break;
        }
    }
    return {creaking.x, static_cast<double>(surface), creaking.z};
}

/// parity: `Mob.getMaxFallDistance`: three, or what its health can spare while it has a target.
[[nodiscard]] std::int32_t maxFallDistance(const ServerEntity& creaking, std::int32_t difficulty) {
    float spare = 0.0F;
    if (creaking.targetId.has_value()) {
        const auto maxHealth = static_cast<float>(creaking.attributeWithModifiers(attribute::kMaxHealth));
        std::int32_t sacrifice = static_cast<std::int32_t>(static_cast<float>(creaking.health) - (maxHealth * 0.33F));
        sacrifice -= (3 - difficulty) * 4;
        spare = static_cast<float>(std::max(sacrifice, 0));
    }
    return static_cast<std::int32_t>(std::floor(spare + 3.0F));
}

/// parity: `Path.getEntityPosAtNode`, the middle of the squares the mob's width covers.
[[nodiscard]] Vec entityPosAt(const ServerEntity& creaking, const Block& node) {
    const double offset =
        static_cast<double>(static_cast<std::int32_t>(static_cast<float>(creaking.width()) + 1.0F)) * 0.5;
    return {node[0] + offset, static_cast<double>(node[1]), node[2] + offset};
}

/// parity: `PathNavigation.getGroundY`.
[[nodiscard]] double groundY(const world::WorldState& world, const Vec& target) {
    const std::int32_t x = floorOf(target[0]);
    const std::int32_t y = floorOf(target[1]);
    const std::int32_t z = floorOf(target[2]);
    return isAirAt(world, x, y - 1, z) ? target[1] : walkNodes::floorLevel(world, x, y, z);
}

/// parity: `GroundPathNavigation.findSurfacePosition`, within the target's own column.
[[nodiscard]] Block surfaceOf(const world::WorldState& world, Block at, std::int32_t minY, std::int32_t maxY) {
    if (isAirAt(world, at[0], at[1], at[2])) {
        std::int32_t column = at[1] - 1;
        while (column >= minY && isAirAt(world, at[0], column, at[2])) {
            --column;
        }
        if (column >= minY) {
            return {at[0], column + 1, at[2]};
        }
        column = at[1] + 1;
        while (column <= maxY && isAirAt(world, at[0], column, at[2])) {
            ++column;
        }
        at[1] = column;
    }
    if (!isSolidAt(world, at[0], at[1], at[2])) {
        return at;
    }
    std::int32_t column = at[1] + 1;
    while (column <= maxY && isSolidAt(world, at[0], column, at[2])) {
        ++column;
    }
    return {at[0], column, at[2]};
}

[[nodiscard]] bool isCauldron(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    const wire::common::Identifier* block =
        state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr) {
        return false;
    }
    // parity: `BlockTags.CAULDRONS`.
    const std::string_view path = block->path();
    return path == "cauldron" || path == "water_cauldron" || path == "lava_cauldron" || path == "powder_snow_cauldron";
}

/// parity: `PathNavigation.trimPath`: a cauldron on the way is stepped over, not into.
void trimPath(const world::WorldState& world, std::vector<Block>& nodes, std::vector<std::uint8_t>& types) {
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const Block node = nodes[index];
        if (!isCauldron(world, node)) {
            continue;
        }
        nodes[index] = {node[0], node[1] + 1, node[2]};
        if (index + 1 < nodes.size() && node[1] >= nodes[index + 1][1]) {
            // parity: `cloneAndMove` of the cauldron's node, so the next one takes its type too.
            nodes[index + 1] = {nodes[index + 1][0], node[1] + 1, nodes[index + 1][2]};
            types[index + 1] = types[index];
        }
    }
}

/// parity: `PathNavigation.canCutCorner`.
[[nodiscard]] bool canCutCorner(std::uint8_t type) {
    const auto pathType = static_cast<PathType>(type);
    return pathType != PathType::FireInNeighbor && pathType != PathType::DamagingInNeighbor &&
           pathType != PathType::WalkableDoor;
}

[[nodiscard]] double lengthSquared(const Vec& vector) {
    return (vector[0] * vector[0]) + (vector[1] * vector[1]) + (vector[2] * vector[2]);
}

/// parity: `Vec3.normalize`, nothing at all under a float 1.0E-5.
[[nodiscard]] Vec normalized(const Vec& vector) {
    const double length = std::sqrt(lengthSquared(vector));
    if (length < static_cast<double>(1.0E-5F)) {
        return {0.0, 0.0, 0.0};
    }
    return {vector[0] / length, vector[1] / length, vector[2] / length};
}

/// parity: `PathNavigation.shouldTargetNextNodeInDirection`, never with a direct line on the ground.
[[nodiscard]] bool shouldTargetNextNode(const ServerEntity& creaking, const Vec& position) {
    const std::vector<Block>& path = *creaking.path;
    const auto next = static_cast<std::size_t>(creaking.pathIndex);
    if (next + 1 >= path.size()) {
        return false;
    }
    const Vec current{path[next][0] + 0.5, static_cast<double>(path[next][1]), path[next][2] + 0.5};
    const Vec toCurrent{current[0] - position[0], current[1] - position[1], current[2] - position[2]};
    if (lengthSquared(toCurrent) >= 4.0) {
        return false;
    }
    const Vec after{path[next + 1][0] + 0.5, static_cast<double>(path[next + 1][1]), path[next + 1][2] + 0.5};
    const Vec toNext{after[0] - position[0], after[1] - position[1], after[2] - position[2]};
    const bool closerToNext = lengthSquared(toNext) < lengthSquared(toCurrent);
    const bool withinCurrent = lengthSquared(toCurrent) < 0.5;
    if (!closerToNext && !withinCurrent) {
        return false;
    }
    const Vec mobDirection = normalized(toCurrent);
    const Vec pathDirection = normalized(toNext);
    return (pathDirection[0] * mobDirection[0]) + (pathDirection[1] * mobDirection[1]) +
               (pathDirection[2] * mobDirection[2]) <
           0.0;
}

/// parity: `PathNavigation.followThePath`.
void followThePath(ServerEntity& creaking, const creakings::CreakingState& state, const Vec& position) {
    const auto width = static_cast<float>(creaking.width());
    const float reach = width > 0.75F ? width / 2.0F : 0.75F - (width / 2.0F);
    const Block& node = (*creaking.path)[static_cast<std::size_t>(creaking.pathIndex)];
    const double dx = std::abs(creaking.x - (node[0] + 0.5));
    const double dy = std::abs(creaking.y - node[1]);
    const double dz = std::abs(creaking.z - (node[2] + 0.5));
    const bool close = dx < static_cast<double>(reach) && dz < static_cast<double>(reach) && dy < 1.0;
    const std::uint8_t type = state.pathNodeTypes[static_cast<std::size_t>(creaking.pathIndex)];
    if (close || (canCutCorner(type) && shouldTargetNextNode(creaking, position))) {
        creaking.pathIndex++;
    }
    ai::navigation::detectStuck(creaking, position, state.gameTime);
}

/// parity: `walkNodes` as the creaking prepares them.
[[nodiscard]] walkNodes::WalkMob walker(world::WorldState& world, const ServerEntity& creaking,
                                        const creakings::CreakingState& state, std::int32_t minY) {
    walkNodes::WalkMob mob{.x = creaking.x,
                           .y = creaking.y,
                           .z = creaking.z,
                           .width = static_cast<float>(creaking.width()),
                           .height = static_cast<float>(creaking.height()),
                           .onGround = creaking.onGround,
                           .inFloatableFluid = ai::inWater(world, creaking),
                           .maxUpStep = static_cast<float>(creaking.attributeWithModifiers(kStepHeight)),
                           .maxFallDistance = maxFallDistance(creaking, state.difficulty),
                           .maluses = maluses(state.home.has_value()),
                           // parity: `setCanFloat(true)` in the constructor, `setCanPassDoors(true)` in
                           // `createPathFinder`.
                           .canFloat = true,
                           .canPassDoors = true,
                           .minY = minY};
    mob.home = state.home;
    return mob;
}

void clearPath(ServerEntity& creaking, creakings::CreakingState& state) {
    ai::navigation::stop(creaking);
    state.pathNodeTypes.clear();
}

} // namespace

pathTypes::Maluses maluses(bool heartBound) {
    pathTypes::Maluses costs = pathTypes::defaults();
    if (heartBound) {
        costs[static_cast<std::size_t>(PathType::Damaging)] = 8.0F;
        costs[static_cast<std::size_t>(PathType::PowderSnow)] = 8.0F;
        costs[static_cast<std::size_t>(PathType::Lava)] = 8.0F;
        costs[static_cast<std::size_t>(PathType::Fire)] = 0.0F;
        costs[static_cast<std::size_t>(PathType::FireInNeighbor)] = 0.0F;
    }
    return costs;
}

bool moveTo(world::WorldState& world, ServerEntity& creaking, double x, double y, double z, std::int32_t reachRange,
            double speedModifier) {
    creakings::CreakingState& state = creakings::stateOf(creaking);
    // parity: `GroundPathNavigation.createPath`: nothing towards a chunk that is not loaded.
    Block target{floorOf(x), floorOf(y), floorOf(z)};
    const server::world::Chunk* column = world.chunkIfLoaded(target[0] >> 4, target[2] >> 4);
    const server::world::Chunk* own = world.chunkIfLoaded(floorOf(creaking.x) >> 4, floorOf(creaking.z) >> 4);
    const std::int32_t minY = own != nullptr ? own->verticalBounds().minY() : kFallbackMinY;
    bool drawn = false;
    if (column != nullptr) {
        target = surfaceOf(world, target, column->verticalBounds().minY(), column->verticalBounds().maxY());
        // parity: `PathNavigation.createPath`: the path it walks is kept while it still leads there.
        const bool current = creaking.path.has_value() && !ai::navigation::isDone(creaking) &&
                             state.pathTarget == target && state.pathNodeTypes.size() == creaking.path->size();
        if (creaking.y >= static_cast<double>(minY) && canUpdatePath(world, creaking)) {
            drawn = true;
        }
        if (drawn && !current) {
            const float maxPathLength =
                std::max(static_cast<float>(creaking.attributeWithModifiers(kFollowRange)), kRequiredPathLength);
            const auto maxVisited = static_cast<std::int32_t>(std::floor(creaking.attribute(kFollowRange) * 16.0));
            walkNodes::Path path = walkNodes::findPath(world, walker(world, creaking, state, minY), target,
                                                       maxPathLength, reachRange, maxVisited);
            state.pathTarget = path.target;
            creaking.navigationWatch.timeoutNode = {};
            creaking.navigationWatch.timeoutTimer = 0;
            creaking.navigationWatch.timeoutLimit = 0.0;
            creaking.navigationWatch.stuck = false;
            std::vector<Block> nodes;
            std::vector<std::uint8_t> types;
            for (const walkNodes::Node& node : path.nodes) {
                nodes.push_back(node.at);
                types.push_back(static_cast<std::uint8_t>(node.type));
            }
            // parity: `moveTo(Path)` keeps the path it has, and how far along it is, when the new one is the same.
            if (!creaking.path.has_value() || *creaking.path != nodes ||
                state.pathNodeTypes.size() != creaking.path->size()) {
                creaking.path = std::move(nodes);
                creaking.pathIndex = 0;
                state.pathNodeTypes = std::move(types);
            }
        }
    }
    if (!drawn) {
        clearPath(creaking, state);
        return false;
    }
    if (ai::navigation::isDone(creaking)) {
        return false;
    }
    trimPath(world, *creaking.path, state.pathNodeTypes);
    if (creaking.path->empty()) {
        return false;
    }
    creaking.moveSpeed = speedModifier;
    creaking.walkTarget = Vec{x, y, z};
    creaking.navigationWatch.lastStuckCheck = creaking.navigationWatch.tick;
    creaking.navigationWatch.lastStuckCheckPos = tempPosition(world, creaking);
    return true;
}

void tick(world::WorldState& world, ServerEntity& creaking) {
    creakings::CreakingState& state = creakings::stateOf(creaking);
    // parity: `CreakingPathNavigation.tick`, nothing at all while it is frozen, not even the count.
    if (!state.canMove) {
        return;
    }
    creaking.navigationWatch.tick++;
    if (!creaking.path.has_value()) {
        return;
    }
    if (ai::navigation::isDone(creaking) || state.pathNodeTypes.size() != creaking.path->size()) {
        clearPath(creaking, state);
        return;
    }
    const Vec position = tempPosition(world, creaking);
    if (canUpdatePath(world, creaking)) {
        followThePath(creaking, state, position);
    } else {
        // parity: in the air it only passes a node it has risen above in the same column.
        const Vec next = entityPosAt(creaking, (*creaking.path)[static_cast<std::size_t>(creaking.pathIndex)]);
        if (position[1] > next[1] && !creaking.onGround && floorOf(position[0]) == floorOf(next[0]) &&
            floorOf(position[2]) == floorOf(next[2])) {
            creaking.pathIndex++;
        }
    }
    if (!creaking.path.has_value() || ai::navigation::isDone(creaking)) {
        clearPath(creaking, state);
        return;
    }
    const Vec target = entityPosAt(creaking, (*creaking.path)[static_cast<std::size_t>(creaking.pathIndex)]);
    ai::moveControl::setWanted(creaking, target[0], groundY(world, target), target[2], creaking.moveSpeed);
}

} // namespace basalt::server::runtime::entity::creakingNavigation

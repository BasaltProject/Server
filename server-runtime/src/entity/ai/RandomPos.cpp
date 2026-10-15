#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace basalt::server::runtime::entity::ai::randomPos {
namespace {

namespace reg = server::registry;

using Block = std::array<std::int32_t, 3>;

constexpr std::int32_t kAttempts = 10;
constexpr float kSqrtOfTwo = 1.4142135F;
constexpr double kQuarterTurn = static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F);

[[nodiscard]] std::int32_t floorOf(double value) { return static_cast<std::int32_t>(std::floor(value)); }

[[nodiscard]] std::string_view pathAt(world::WorldState& world, const Block& pos) {
    const std::optional<std::int32_t> state = world.blockState(pos[0], pos[1], pos[2]);
    if (!state.has_value()) {
        return {};
    }
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*state);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

/// parity: `GoalUtils.mobRestricted`.
[[nodiscard]] bool restricted(const ServerEntity& mob, double horizontal) {
    if (!mob.homePosition.has_value()) {
        return false;
    }
    const auto& home = *mob.homePosition;
    const double dx = home[0] + 0.5 - mob.x;
    const double dy = home[1] + 0.5 - mob.y;
    const double dz = home[2] + 0.5 - mob.z;
    const double reach = static_cast<double>(mob.homeRadius) + horizontal + 1.0;
    return (dx * dx) + (dy * dy) + (dz * dz) < reach * reach;
}

[[nodiscard]] bool withinHome(const ServerEntity& mob, const Block& pos) {
    if (!mob.homePosition.has_value()) {
        return true;
    }
    const auto& home = *mob.homePosition;
    const auto dx = static_cast<double>(home[0] - pos[0]);
    const auto dy = static_cast<double>(home[1] - pos[1]);
    const auto dz = static_cast<double>(home[2] - pos[2]);
    const auto radius = static_cast<double>(mob.homeRadius);
    return (dx * dx) + (dy * dy) + (dz * dz) < radius * radius;
}

/// parity: `PathNavigation.isStableDestination`, a solid render block underneath.
[[nodiscard]] bool stable(world::WorldState& world, const Block& pos) {
    const std::optional<std::int32_t> below = world.blockState(pos[0], pos[1] - 1, pos[2]);
    return below.has_value() && reg::blockShapes::isSolidRender(*below);
}

/// parity: `GoalUtils.hasMalus`.
[[nodiscard]] bool hasMalus(world::WorldState& world, const Block& pos) {
    const std::optional<std::int32_t> state = world.blockState(pos[0], pos[1], pos[2]);
    if (!state.has_value()) {
        return true;
    }
    const std::string_view path = pathAt(world, pos);
    if (path == "water" || path == "lava" || path == "fire" || path == "soul_fire" || path == "powder_snow" ||
        path == "sweet_berry_bush" || path == "cobweb") {
        return true;
    }
    return !reg::blockShapes::isEmptyShape(*state) && !pathFinder::isMobDoor(*state);
}

[[nodiscard]] bool solid(world::WorldState& world, const Block& pos) {
    const std::optional<std::int32_t> state = world.blockState(pos[0], pos[1], pos[2]);
    return state.has_value() && reg::blockShapes::isSolidState(*state);
}

/// parity: `RandomPos.generateRandomDirection`.
[[nodiscard]] Block direction(random::RandomSource& random, std::int32_t horizontal, std::int32_t vertical) {
    const std::int32_t x = random.nextInt((2 * horizontal) + 1) - horizontal;
    const std::int32_t y = random.nextInt((2 * vertical) + 1) - vertical;
    const std::int32_t z = random.nextInt((2 * horizontal) + 1) - horizontal;
    return {x, y, z};
}

/// parity: `RandomPos.generateRandomDirectionWithinRadians`.
[[nodiscard]] std::optional<Block> directionWithin(random::RandomSource& random, double minHorizontal,
                                                   double maxHorizontal, std::int32_t vertical, double dirX,
                                                   double dirZ, double maxRadians) {
    const double center = std::atan2(dirZ, dirX) - static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F);
    const double yaw = center + static_cast<double>((2.0F * random.nextFloat()) - 1.0F) * maxRadians;
    const double t = std::sqrt(random.nextDouble());
    const double dist = (minHorizontal + (t * (maxHorizontal - minHorizontal))) * static_cast<double>(kSqrtOfTwo);
    const double x = -dist * std::sin(yaw);
    const double z = dist * std::cos(yaw);
    if (std::abs(x) > maxHorizontal || std::abs(z) > maxHorizontal) {
        return std::nullopt;
    }
    const std::int32_t y = random.nextInt((2 * vertical) + 1) - vertical;
    return Block{floorOf(x), y, floorOf(z)};
}

/// parity: `RandomPos.generateRandomPosTowardDirection`, pulled back towards home.
[[nodiscard]] Block towardDirection(const ServerEntity& mob, double horizontal, random::RandomSource& random,
                                    const Block& offset) {
    double x = offset[0];
    double z = offset[2];
    if (mob.homePosition.has_value() && horizontal > 1.0) {
        const auto& home = *mob.homePosition;
        x += (mob.x > home[0] ? -1.0 : 1.0) * random.nextDouble() * horizontal / 2.0;
        z += (mob.z > home[2] ? -1.0 : 1.0) * random.nextDouble() * horizontal / 2.0;
    }
    return {floorOf(x + mob.x), floorOf(static_cast<double>(offset[1]) + mob.y), floorOf(z + mob.z)};
}

/// parity: `generateRandomPosTowardDirection` of both classes.
[[nodiscard]] std::optional<Block> accept(world::WorldState& world, const ServerEntity& mob, double horizontal,
                                          bool restrict, const Block& offset, random::RandomSource& random,
                                          bool checkMalus) {
    const Block pos = towardDirection(mob, horizontal, random, offset);
    if (!world.blockState(pos[0], pos[1], pos[2]).has_value() || (restrict && !withinHome(mob, pos)) ||
        !stable(world, pos) || (checkMalus && hasMalus(world, pos))) {
        return std::nullopt;
    }
    return pos;
}

/// parity: `LandRandomPos.movePosUpOutOfSolid`.
[[nodiscard]] std::optional<Block> upOutOfSolid(world::WorldState& world, Block pos) {
    if (solid(world, pos)) {
        pos[1]++;
        while (world.blockState(pos[0], pos[1], pos[2]).has_value() && solid(world, pos)) {
            pos[1]++;
        }
    }
    if (pathAt(world, pos) == "water" || hasMalus(world, pos)) {
        return std::nullopt;
    }
    return pos;
}

template <typename Supplier>
[[nodiscard]] std::optional<Spot> best(const Supplier& supplier, const Weight& weight) {
    double bestWeight = -std::numeric_limits<double>::infinity();
    std::optional<Block> chosen;
    for (std::int32_t attempt = 0; attempt < kAttempts; ++attempt) {
        const std::optional<Block> pos = supplier();
        if (!pos.has_value()) {
            continue;
        }
        const double value = weight ? weight((*pos)[0], (*pos)[1], (*pos)[2]) : 0.0;
        if (value > bestWeight) {
            bestWeight = value;
            chosen = pos;
        }
    }
    if (!chosen.has_value()) {
        return std::nullopt;
    }
    return Spot{(*chosen)[0] + 0.5, static_cast<double>((*chosen)[1]), (*chosen)[2] + 0.5};
}

} // namespace

std::optional<Spot> defaultPos(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                               std::int32_t horizontal, std::int32_t vertical) {
    const bool restrict = restricted(mob, horizontal);
    return best(
        [&]() -> std::optional<Block> {
            return accept(world, mob, horizontal, restrict, direction(random, horizontal, vertical), random, true);
        },
        {});
}

std::optional<Spot> defaultPosTowards(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                                      std::int32_t horizontal, std::int32_t vertical, const Spot& towards,
                                      double maxRadians) {
    const double dirX = towards[0] - mob.x;
    const double dirZ = towards[2] - mob.z;
    const bool restrict = restricted(mob, horizontal);
    return best(
        [&]() -> std::optional<Block> {
            const std::optional<Block> offset =
                directionWithin(random, 0.0, horizontal, vertical, dirX, dirZ, maxRadians);
            if (!offset.has_value()) {
                return std::nullopt;
            }
            return accept(world, mob, horizontal, restrict, *offset, random, true);
        },
        {});
}

std::optional<Spot> defaultPosAway(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                                   std::int32_t horizontal, std::int32_t vertical, const Spot& away) {
    const double dirX = mob.x - away[0];
    const double dirZ = mob.z - away[2];
    const bool restrict = restricted(mob, horizontal);
    return best(
        [&]() -> std::optional<Block> {
            const std::optional<Block> offset =
                directionWithin(random, 0.0, horizontal, vertical, dirX, dirZ, kQuarterTurn);
            if (!offset.has_value()) {
                return std::nullopt;
            }
            return accept(world, mob, horizontal, restrict, *offset, random, true);
        },
        {});
}

std::optional<Spot> landPos(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                            std::int32_t horizontal, std::int32_t vertical, const Weight& weight) {
    const bool restrict = restricted(mob, horizontal);
    return best(
        [&]() -> std::optional<Block> {
            const std::optional<Block> pos =
                accept(world, mob, horizontal, restrict, direction(random, horizontal, vertical), random, false);
            return pos.has_value() ? upOutOfSolid(world, *pos) : std::nullopt;
        },
        weight);
}

namespace {

[[nodiscard]] std::optional<Spot> landInDirection(world::WorldState& world, const ServerEntity& mob,
                                                  random::RandomSource& random, double maxHorizontal,
                                                  std::int32_t vertical, double dirX, double dirZ, bool restrict) {
    return best(
        [&]() -> std::optional<Block> {
            const std::optional<Block> offset =
                directionWithin(random, 0.0, maxHorizontal, vertical, dirX, dirZ, kQuarterTurn);
            if (!offset.has_value()) {
                return std::nullopt;
            }
            const std::optional<Block> pos = accept(world, mob, maxHorizontal, restrict, *offset, random, false);
            return pos.has_value() ? upOutOfSolid(world, *pos) : std::nullopt;
        },
        {});
}

} // namespace

std::optional<Spot> landPosTowards(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                                   std::int32_t horizontal, std::int32_t vertical, const Spot& towards) {
    return landInDirection(world, mob, random, horizontal, vertical, towards[0] - mob.x, towards[2] - mob.z,
                           restricted(mob, horizontal));
}

std::optional<Spot> landPosAway(world::WorldState& world, const ServerEntity& mob, random::RandomSource& random,
                                std::int32_t horizontal, std::int32_t vertical, const Spot& away) {
    double dirX = mob.x - away[0];
    double dirZ = mob.z - away[2];
    const double dirY = mob.y - away[1];
    if (std::sqrt((dirX * dirX) + (dirY * dirY) + (dirZ * dirZ)) == 0.0) {
        dirX = random.nextDouble() - 0.5;
        dirZ = random.nextDouble() - 0.5;
    }
    return landInDirection(world, mob, random, horizontal, vertical, dirX, dirZ, restricted(mob, horizontal));
}

} // namespace basalt::server::runtime::entity::ai::randomPos

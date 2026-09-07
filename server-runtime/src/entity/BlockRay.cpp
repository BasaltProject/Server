#include <basalt/server/runtime/entity/BlockRay.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::server::runtime::entity::blockRay {
namespace {

constexpr double kUnits = 32.0;

/// How far it is from `from` to the next block boundary in the direction it is going.
[[nodiscard]] double boundary(double from, double delta) {
    const double block = std::floor(from);
    return delta > 0 ? block + 1.0 - from : block - from;
}

/// The face a ray going this way enters through, which is the one it is coming at.
[[nodiscard]] registry::BlockFace faceOf(std::int32_t axis, bool positive) {
    if (axis == 0) {
        return positive ? registry::BlockFace::West : registry::BlockFace::East;
    }
    if (axis == 1) {
        return positive ? registry::BlockFace::Down : registry::BlockFace::Up;
    }
    return positive ? registry::BlockFace::North : registry::BlockFace::South;
}

/// parity: `AABB.clip` against each box of the block's shape, keeping the nearest.
[[nodiscard]] std::optional<BlockHit> hitInBlock(world::WorldState& world, std::int32_t blockX, std::int32_t blockY,
                                                 std::int32_t blockZ, double fromX, double fromY, double fromZ,
                                                 double dx, double dy, double dz) {
    const std::optional<std::int32_t> stateId = world.blockState(blockX, blockY, blockZ);
    if (!stateId.has_value() || registry::blockShapes::isEmptyShape(*stateId)) {
        return std::nullopt;
    }
    std::optional<BlockHit> best;
    double bestScale = std::numeric_limits<double>::max();
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(*stateId);
    for (std::size_t box = 0; box + 5 < boxes.size(); box += 6) {
        const std::array<double, 3> low{blockX + (boxes[box] / kUnits), blockY + (boxes[box + 1] / kUnits),
                                        blockZ + (boxes[box + 2] / kUnits)};
        const std::array<double, 3> high{blockX + (boxes[box + 3] / kUnits), blockY + (boxes[box + 4] / kUnits),
                                         blockZ + (boxes[box + 5] / kUnits)};
        const std::array<double, 3> origins{fromX, fromY, fromZ};
        const std::array<double, 3> deltas{dx, dy, dz};

        double enter = 0.0;
        double exit = 1.0;
        std::optional<registry::BlockFace> face;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            if (deltas[axis] == 0.0) {
                if (origins[axis] < low[axis] || origins[axis] > high[axis]) {
                    enter = std::numeric_limits<double>::max();
                    break;
                }
                continue;
            }
            const double toLow = (low[axis] - origins[axis]) / deltas[axis];
            const double toHigh = (high[axis] - origins[axis]) / deltas[axis];
            const double near = std::min(toLow, toHigh);
            const double far = std::max(toLow, toHigh);
            if (near > enter) {
                enter = near;
                face = faceOf(static_cast<std::int32_t>(axis), deltas[axis] > 0);
            }
            exit = std::min(exit, far);
            if (enter > exit) {
                enter = std::numeric_limits<double>::max();
                break;
            }
        }
        if (enter == std::numeric_limits<double>::max() || enter > 1.0 || enter >= bestScale) {
            continue;
        }
        bestScale = enter;
        best = BlockHit{.x = fromX + (dx * enter),
                        .y = fromY + (dy * enter),
                        .z = fromZ + (dz * enter),
                        .blockX = blockX,
                        .blockY = blockY,
                        .blockZ = blockZ,
                        .face = face.value_or(registry::BlockFace::Up)};
    }
    return best;
}

} // namespace

std::optional<BlockHit> cast(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                             double toZ) {
    const double dx = toX - fromX;
    const double dy = toY - fromY;
    const double dz = toZ - fromZ;
    if (dx == 0.0 && dy == 0.0 && dz == 0.0) {
        return std::nullopt;
    }

    auto blockX = static_cast<std::int32_t>(std::floor(fromX));
    auto blockY = static_cast<std::int32_t>(std::floor(fromY));
    auto blockZ = static_cast<std::int32_t>(std::floor(fromZ));
    const auto endX = static_cast<std::int32_t>(std::floor(toX));
    const auto endY = static_cast<std::int32_t>(std::floor(toY));
    const auto endZ = static_cast<std::int32_t>(std::floor(toZ));

    const std::int32_t stepX = dx > 0 ? 1 : -1;
    const std::int32_t stepY = dy > 0 ? 1 : -1;
    const std::int32_t stepZ = dz > 0 ? 1 : -1;
    // How far along the line one whole block of each axis is, and how far to the next boundary.
    const double deltaX = dx == 0.0 ? std::numeric_limits<double>::max() : std::abs(1.0 / dx);
    const double deltaY = dy == 0.0 ? std::numeric_limits<double>::max() : std::abs(1.0 / dy);
    const double deltaZ = dz == 0.0 ? std::numeric_limits<double>::max() : std::abs(1.0 / dz);
    double nextX = dx == 0.0 ? std::numeric_limits<double>::max() : boundary(fromX, dx) / dx;
    double nextY = dy == 0.0 ? std::numeric_limits<double>::max() : boundary(fromY, dy) / dy;
    double nextZ = dz == 0.0 ? std::numeric_limits<double>::max() : boundary(fromZ, dz) / dz;

    for (std::int32_t steps = 0; steps < kMaxSteps; ++steps) {
        if (const std::optional<BlockHit> hit =
                hitInBlock(world, blockX, blockY, blockZ, fromX, fromY, fromZ, dx, dy, dz);
            hit.has_value()) {
            return hit;
        }
        if (blockX == endX && blockY == endY && blockZ == endZ) {
            return std::nullopt;
        }
        if (nextX < nextY && nextX < nextZ) {
            blockX += stepX;
            nextX += deltaX;
        } else if (nextY < nextZ) {
            blockY += stepY;
            nextY += deltaY;
        } else {
            blockZ += stepZ;
            nextZ += deltaZ;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::entity::blockRay

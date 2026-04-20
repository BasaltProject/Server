#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity::entityCollision {
namespace {

enum class Axis : std::uint8_t { X, Y, Z };

const double kUnits = static_cast<double>(registry::blockShapes::kUnits);

template <typename Action>
void forEachBox(const BlockAt& blockAt, const Aabb& box, const ShapeFor& shapeFor, Action&& action) {
    const std::int32_t minX = static_cast<std::int32_t>(std::floor(box.minX));
    const std::int32_t maxX = static_cast<std::int32_t>(std::ceil(box.maxX));
    const std::int32_t minY = static_cast<std::int32_t>(std::floor(box.minY));
    const std::int32_t maxY = static_cast<std::int32_t>(std::ceil(box.maxY));
    const std::int32_t minZ = static_cast<std::int32_t>(std::floor(box.minZ));
    const std::int32_t maxZ = static_cast<std::int32_t>(std::ceil(box.maxZ));
    // A box this big is a bug upstream: gathering its blocks would cost more than it could be worth.
    if (maxX - minX > kMaxSpan || maxY - minY > kMaxSpan || maxZ - minZ > kMaxSpan) {
        return;
    }
    for (std::int32_t y = minY; y <= maxY; ++y) {
        for (std::int32_t z = minZ; z <= maxZ; ++z) {
            for (std::int32_t x = minX; x <= maxX; ++x) {
                const std::optional<std::int32_t> stateId = blockAt(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                if (shapeFor) {
                    if (const std::optional<Aabb> instead = shapeFor(*stateId, x, y, z); instead.has_value()) {
                        action(*instead);
                        continue;
                    }
                }
                if (registry::blockShapes::isEmptyShape(*stateId)) {
                    continue;
                }
                const std::span<const std::int32_t> shapes = registry::blockShapes::collisionBoxes(*stateId);
                for (std::size_t at = 0; at + 5 < shapes.size(); at += 6) {
                    action(Aabb{.minX = x + shapes[at] / kUnits,
                                .minY = y + shapes[at + 1] / kUnits,
                                .minZ = z + shapes[at + 2] / kUnits,
                                .maxX = x + shapes[at + 3] / kUnits,
                                .maxY = y + shapes[at + 4] / kUnits,
                                .maxZ = z + shapes[at + 5] / kUnits});
                }
            }
        }
    }
}

[[nodiscard]] std::pair<double, double> spanOf(Axis axis, const Aabb& box) {
    switch (axis) {
    case Axis::X:
        return {box.minX, box.maxX};
    case Axis::Y:
        return {box.minY, box.maxY};
    case Axis::Z:
    default:
        return {box.minZ, box.maxZ};
    }
}

/// Counts only when overlapping the other two axes by more than epsilon, so sliding along a wall is no hit.
[[nodiscard]] bool overlapsOtherAxes(Axis axis, const Aabb& box, const Aabb& collider) {
    if (axis != Axis::X && !(box.minX < collider.maxX - kEpsilon && box.maxX > collider.minX + kEpsilon)) {
        return false;
    }
    if (axis != Axis::Y && !(box.minY < collider.maxY - kEpsilon && box.maxY > collider.minY + kEpsilon)) {
        return false;
    }
    if (axis != Axis::Z && !(box.minZ < collider.maxZ - kEpsilon && box.maxZ > collider.minZ + kEpsilon)) {
        return false;
    }
    return true;
}

[[nodiscard]] double clipOne(Axis axis, const Aabb& box, const Aabb& collider, double distance) {
    const auto [movingMin, movingMax] = spanOf(axis, box);
    const auto [colliderMin, colliderMax] = spanOf(axis, collider);
    if (!overlapsOtherAxes(axis, box, collider)) {
        return distance;
    }
    if (distance > 0.0 && colliderMin >= movingMax - kEpsilon) {
        // Blocks are gathered whole, so a collider can lie beyond the movement: the answer is the move, not the gap.
        return std::min(distance, colliderMin - movingMax);
    }
    if (distance < 0.0 && colliderMax <= movingMin + kEpsilon) {
        return std::max(distance, colliderMax - movingMin);
    }
    return distance;
}

/// parity: `AABB.collide`.
[[nodiscard]] double clip(Axis axis, const Aabb& box, const std::vector<Aabb>& boxes, double distance) {
    double left = distance;
    for (const Aabb& collider : boxes) {
        if (std::abs(left) < kEpsilon) {
            return 0.0;
        }
        left = clipOne(axis, box, collider, left);
    }
    return left;
}

/// parity: `collectCandidateStepUpHeights`.
[[nodiscard]] std::vector<double> candidateHeights(const BlockAt& blockAt, const Aabb& grounded, double dx, double dz,
                                                   double stepHeight, double skip) {
    const Aabb reach = grounded.expandTowards(dx, stepHeight, dz);
    std::vector<double> heights;
    for (const Aabb& collider : collidingBoxes(blockAt, reach)) {
        for (const double edge : {collider.minY, collider.maxY}) {
            const double height = edge - grounded.minY;
            // A step is a step up: the floor's own edges measure zero or less from the feet.
            if (height <= 0.0 || height > stepHeight || height == skip) {
                continue;
            }
            if (std::ranges::find(heights, height) == heights.end()) {
                heights.push_back(height);
            }
        }
    }
    std::ranges::sort(heights);
    return heights;
}

} // namespace

std::array<double, 3> collide(const BlockAt& blockAt, const Aabb& box, double dx, double dy, double dz,
                              const ShapeFor& shapeFor) {
    std::array<double, 3> out{dx, dy, dz};
    if (dx == 0.0 && dy == 0.0 && dz == 0.0) {
        return out;
    }
    const std::vector<Aabb> boxes = collidingBoxes(blockAt, box.expandTowards(dx, dy, dz), shapeFor);
    if (boxes.empty()) {
        return out;
    }

    double movedX = 0.0;
    double movedY = 0.0;
    double movedZ = 0.0;
    // parity: `Direction.axisStepOrder`, Y first and the bigger horizontal move last.
    if (dy != 0.0) {
        movedY = clip(Axis::Y, box.move(movedX, movedY, movedZ), boxes, dy);
    }
    if (std::abs(dx) >= std::abs(dz)) {
        if (dz != 0.0) {
            movedZ = clip(Axis::Z, box.move(movedX, movedY, movedZ), boxes, dz);
        }
        if (dx != 0.0) {
            movedX = clip(Axis::X, box.move(movedX, movedY, movedZ), boxes, dx);
        }
    } else {
        if (dx != 0.0) {
            movedX = clip(Axis::X, box.move(movedX, movedY, movedZ), boxes, dx);
        }
        if (dz != 0.0) {
            movedZ = clip(Axis::Z, box.move(movedX, movedY, movedZ), boxes, dz);
        }
    }
    return std::array<double, 3>{movedX, movedY, movedZ};
}

std::array<double, 3> collideWithStep(const BlockAt& blockAt, const Aabb& box, double dx, double dy, double dz,
                                      double stepHeight, bool onGround) {
    std::array<double, 3> out = collide(blockAt, box, dx, dy, dz);
    if (stepHeight <= 0.0) {
        return out;
    }
    const double flatX = out[0];
    const double flatY = out[1];
    const double flatZ = out[2];
    if (flatX == dx && flatZ == dz) {
        return out;
    }
    // parity: `movement.y != adjusted.y && movement.y < 0.0`.
    const bool landed = flatY != dy && dy < 0.0;
    if (!onGround && !landed) {
        return out;
    }

    const Aabb grounded = landed ? box.move(0.0, flatY, 0.0) : box;
    const double flatHorizontal = flatX * flatX + flatZ * flatZ;
    for (const double height : candidateHeights(blockAt, grounded, dx, dz, stepHeight, flatY)) {
        const std::array<double, 3> stepped = collide(blockAt, grounded, dx, height, dz);
        if (stepped[0] * stepped[0] + stepped[2] * stepped[2] <= flatHorizontal) {
            continue;
        }
        out[0] = stepped[0];
        // parity: `subtract(0, aabb.minY - groundedAABB.minY, 0)`, the drop already taken.
        out[1] = stepped[1] + (landed ? flatY : 0.0);
        out[2] = stepped[2];
        return out;
    }
    return out;
}

bool collides(const BlockAt& blockAt, const Aabb& box) {
    bool hit = false;
    forEachBox(blockAt, box, {}, [&box, &hit](const Aabb& collider) {
        if (!hit && collider.intersects(box)) {
            hit = true;
        }
    });
    return hit;
}

std::vector<Aabb> collidingBoxes(const BlockAt& blockAt, const Aabb& box, const ShapeFor& shapeFor) {
    std::vector<Aabb> found;
    forEachBox(blockAt, box, shapeFor, [&found](const Aabb& collider) { found.push_back(collider); });
    return found;
}

} // namespace basalt::server::runtime::entity::entityCollision

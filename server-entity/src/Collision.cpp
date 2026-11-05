#include <basalt/server/entity/Collision.hpp>

#include <algorithm>
#include <cmath>
#include <set>

namespace basalt::server::entity::collision {
namespace {
constexpr double kStopBelow = 1.0E-7;

enum class Axis : std::int8_t { X, Y, Z };

double lowOf(const geometry::Box& box, Axis axis) {
    return axis == Axis::X ? box.minX : axis == Axis::Y ? box.minY : box.minZ;
}

double highOf(const geometry::Box& box, Axis axis) {
    return axis == Axis::X ? box.maxX : axis == Axis::Y ? box.maxY : box.maxZ;
}

Axis firstOther(Axis axis) { return axis == Axis::X ? Axis::Y : Axis::X; }

Axis secondOther(Axis axis) { return axis == Axis::Z ? Axis::Y : Axis::Z; }

double reachAlong(const geometry::Box& moving, const geometry::Box& box, Axis axis, double distance) {
    const Axis a = firstOther(axis);
    const Axis b = secondOther(axis);
    if (highOf(moving, a) <= lowOf(box, a) || lowOf(moving, a) >= highOf(box, a) ||
        highOf(moving, b) <= lowOf(box, b) || lowOf(moving, b) >= highOf(box, b)) {
        return distance;
    }
    if (distance > 0 && lowOf(box, axis) >= highOf(moving, axis)) {
        return std::min(distance, lowOf(box, axis) - highOf(moving, axis));
    }
    if (distance < 0 && highOf(box, axis) <= lowOf(moving, axis)) {
        return std::max(distance, highOf(box, axis) - lowOf(moving, axis));
    }
    return distance;
}

double collideAxis(Axis axis, const geometry::Box& moving, std::span<const Collider> colliders, double distance) {
    double remaining = distance;
    for (std::size_t at = 0; at < colliders.size(); ++at) {
        remaining = reachAlong(moving, colliders[at].box, axis, remaining);
        const bool shapeEnds = at + 1 == colliders.size() || colliders[at + 1].shape != colliders[at].shape;
        if (shapeEnds && std::abs(remaining) < kStopBelow) {
            return 0.0;
        }
    }
    return remaining;
}
} // namespace

geometry::Point sweep(std::span<const Collider> colliders, const geometry::Box& box, const geometry::Point& movement) {
    if (colliders.empty()) {
        return movement;
    }
    double x = movement[0];
    double y = movement[1];
    double z = movement[2];
    geometry::Box moving = box;
    if (y != 0.0) {
        y = collideAxis(Axis::Y, moving, colliders, y);
        if (y != 0.0) {
            moving = geometry::move(moving, 0.0, y, 0.0);
        }
    }
    const bool zFirst = std::abs(x) < std::abs(z);
    if (zFirst && z != 0.0) {
        z = collideAxis(Axis::Z, moving, colliders, z);
        if (z != 0.0) {
            moving = geometry::move(moving, 0.0, 0.0, z);
        }
    }
    if (x != 0.0) {
        x = collideAxis(Axis::X, moving, colliders, x);
        if (!zFirst && x != 0.0) {
            moving = geometry::move(moving, x, 0.0, 0.0);
        }
    }
    if (!zFirst && z != 0.0) {
        z = collideAxis(Axis::Z, moving, colliders, z);
    }
    return {x, y, z};
}

std::optional<geometry::Point> stepUp(std::span<const Collider> colliders, const geometry::Box& grounded,
                                      double groundedOffsetY, const geometry::Point& movement, float maxUpStep,
                                      double reachSquared) {
    std::set<double> heights{static_cast<double>(maxUpStep)};
    for (const Collider& collider : colliders) {
        const double rise = collider.box.maxY - grounded.minY;
        if (rise > 0.0 && rise <= static_cast<double>(maxUpStep)) {
            heights.insert(rise);
        }
    }
    for (const double height : heights) {
        const geometry::Point stepped = sweep(colliders, grounded, {movement[0], height, movement[2]});
        if ((stepped[0] * stepped[0]) + (stepped[2] * stepped[2]) > reachSquared) {
            return geometry::Point{stepped[0], stepped[1] + groundedOffsetY, stepped[2]};
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::entity::collision

#pragma once

#include <array>
#include <optional>

namespace basalt::server::entity::geometry {

struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;
};

using Point = std::array<double, 3>;

/// Normalised like the `AABB` constructor, so the corners may come in any order.
[[nodiscard]] Box makeBox(double x0, double y0, double z0, double x1, double y1, double z1);

[[nodiscard]] Box inflate(const Box& box, double amount);

[[nodiscard]] Box expandTowards(const Box& box, double dx, double dy, double dz);

[[nodiscard]] Box contract(const Box& box, double dx, double dy, double dz);

[[nodiscard]] Box move(const Box& box, double dx, double dy, double dz);

[[nodiscard]] bool contains(const Box& box, const Point& point);

/// The first point of the segment inside the box, if the segment crosses it.
[[nodiscard]] std::optional<Point> clip(const Box& box, const Point& from, const Point& to);

[[nodiscard]] double distanceSquared(const Point& a, const Point& b);

} // namespace basalt::server::entity::geometry

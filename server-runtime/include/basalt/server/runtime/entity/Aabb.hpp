#pragma once

namespace basalt::server::runtime::entity {

/// parity: `AABB`.
struct Aabb {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;

    [[nodiscard]] static Aabb of(double x, double y, double z, double width, double height);

    [[nodiscard]] Aabb move(double dx, double dy, double dz) const;

    /// parity: `AABB.expandTowards`.
    [[nodiscard]] Aabb expandTowards(double dx, double dy, double dz) const;

    [[nodiscard]] Aabb inflate(double amount) const;

    [[nodiscard]] bool intersects(const Aabb& other) const;

    [[nodiscard]] bool contains(double x, double y, double z) const;

    friend bool operator==(const Aabb&, const Aabb&) = default;
};

} // namespace basalt::server::runtime::entity

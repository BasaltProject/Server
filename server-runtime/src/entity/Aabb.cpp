#include <basalt/server/runtime/entity/Aabb.hpp>

namespace basalt::server::runtime::entity {

Aabb Aabb::of(double x, double y, double z, double width, double height) {
    const double half = width / 2;
    return Aabb{.minX = x - half, .minY = y, .minZ = z - half, .maxX = x + half, .maxY = y + height, .maxZ = z + half};
}

Aabb Aabb::move(double dx, double dy, double dz) const {
    return Aabb{.minX = minX + dx,
                .minY = minY + dy,
                .minZ = minZ + dz,
                .maxX = maxX + dx,
                .maxY = maxY + dy,
                .maxZ = maxZ + dz};
}

Aabb Aabb::expandTowards(double dx, double dy, double dz) const {
    return Aabb{.minX = dx < 0 ? minX + dx : minX,
                .minY = dy < 0 ? minY + dy : minY,
                .minZ = dz < 0 ? minZ + dz : minZ,
                .maxX = dx > 0 ? maxX + dx : maxX,
                .maxY = dy > 0 ? maxY + dy : maxY,
                .maxZ = dz > 0 ? maxZ + dz : maxZ};
}

Aabb Aabb::inflate(double amount) const {
    return Aabb{.minX = minX - amount,
                .minY = minY - amount,
                .minZ = minZ - amount,
                .maxX = maxX + amount,
                .maxY = maxY + amount,
                .maxZ = maxZ + amount};
}

bool Aabb::intersects(const Aabb& other) const {
    // Touching is not intersecting: two boxes that share a face are side by side, which is what
    // lets an entity stand exactly on a block without being inside it.
    return minX < other.maxX && maxX > other.minX && minY < other.maxY && maxY > other.minY && minZ < other.maxZ &&
           maxZ > other.minZ;
}

bool Aabb::contains(double x, double y, double z) const {
    return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
}

} // namespace basalt::server::runtime::entity

#include <basalt/server/entity/Geometry.hpp>

#include <algorithm>

namespace basalt::server::entity::geometry {
namespace {
constexpr double kClipEpsilon = 1.0E-7;

void clipFace(double& scale, bool& hit, double da, double db, double dc, double plane, double minB, double maxB,
              double minC, double maxC, double fromA, double fromB, double fromC) {
    const double s = (plane - fromA) / da;
    const double pb = fromB + (s * db);
    const double pc = fromC + (s * dc);
    if (0.0 < s && s < scale && minB - kClipEpsilon < pb && pb < maxB + kClipEpsilon && minC - kClipEpsilon < pc &&
        pc < maxC + kClipEpsilon) {
        scale = s;
        hit = true;
    }
}
} // namespace

Box makeBox(double x0, double y0, double z0, double x1, double y1, double z1) {
    return {std::min(x0, x1), std::min(y0, y1), std::min(z0, z1), std::max(x0, x1), std::max(y0, y1), std::max(z0, z1)};
}

Box inflate(const Box& box, double amount) {
    return makeBox(box.minX - amount, box.minY - amount, box.minZ - amount, box.maxX + amount, box.maxY + amount,
                   box.maxZ + amount);
}

Box expandTowards(const Box& box, double dx, double dy, double dz) {
    double x0 = box.minX;
    double y0 = box.minY;
    double z0 = box.minZ;
    double x1 = box.maxX;
    double y1 = box.maxY;
    double z1 = box.maxZ;
    (dx < 0.0 ? x0 : x1) += dx > 0.0 || dx < 0.0 ? dx : 0.0;
    (dy < 0.0 ? y0 : y1) += dy > 0.0 || dy < 0.0 ? dy : 0.0;
    (dz < 0.0 ? z0 : z1) += dz > 0.0 || dz < 0.0 ? dz : 0.0;
    return makeBox(x0, y0, z0, x1, y1, z1);
}

Box contract(const Box& box, double dx, double dy, double dz) {
    double x0 = box.minX;
    double y0 = box.minY;
    double z0 = box.minZ;
    double x1 = box.maxX;
    double y1 = box.maxY;
    double z1 = box.maxZ;
    (dx < 0.0 ? x0 : x1) -= dx > 0.0 || dx < 0.0 ? dx : 0.0;
    (dy < 0.0 ? y0 : y1) -= dy > 0.0 || dy < 0.0 ? dy : 0.0;
    (dz < 0.0 ? z0 : z1) -= dz > 0.0 || dz < 0.0 ? dz : 0.0;
    return makeBox(x0, y0, z0, x1, y1, z1);
}

Box move(const Box& box, double dx, double dy, double dz) {
    return makeBox(box.minX + dx, box.minY + dy, box.minZ + dz, box.maxX + dx, box.maxY + dy, box.maxZ + dz);
}

bool contains(const Box& box, const Point& point) {
    return point[0] >= box.minX && point[0] < box.maxX && point[1] >= box.minY && point[1] < box.maxY &&
           point[2] >= box.minZ && point[2] < box.maxZ;
}

std::optional<Point> clip(const Box& box, const Point& from, const Point& to) {
    double scale = 1.0;
    bool hit = false;
    const double dx = to[0] - from[0];
    const double dy = to[1] - from[1];
    const double dz = to[2] - from[2];
    if (dx > kClipEpsilon) {
        clipFace(scale, hit, dx, dy, dz, box.minX, box.minY, box.maxY, box.minZ, box.maxZ, from[0], from[1], from[2]);
    } else if (dx < -kClipEpsilon) {
        clipFace(scale, hit, dx, dy, dz, box.maxX, box.minY, box.maxY, box.minZ, box.maxZ, from[0], from[1], from[2]);
    }
    if (dy > kClipEpsilon) {
        clipFace(scale, hit, dy, dz, dx, box.minY, box.minZ, box.maxZ, box.minX, box.maxX, from[1], from[2], from[0]);
    } else if (dy < -kClipEpsilon) {
        clipFace(scale, hit, dy, dz, dx, box.maxY, box.minZ, box.maxZ, box.minX, box.maxX, from[1], from[2], from[0]);
    }
    if (dz > kClipEpsilon) {
        clipFace(scale, hit, dz, dx, dy, box.minZ, box.minX, box.maxX, box.minY, box.maxY, from[2], from[0], from[1]);
    } else if (dz < -kClipEpsilon) {
        clipFace(scale, hit, dz, dx, dy, box.maxZ, box.minX, box.maxX, box.minY, box.maxY, from[2], from[0], from[1]);
    }
    if (!hit) {
        return std::nullopt;
    }
    return Point{from[0] + (scale * dx), from[1] + (scale * dy), from[2] + (scale * dz)};
}

double distanceSquared(const Point& a, const Point& b) {
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double dz = b[2] - a[2];
    return (dx * dx) + (dy * dy) + (dz * dz);
}

} // namespace basalt::server::entity::geometry

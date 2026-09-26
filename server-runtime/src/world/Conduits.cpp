#include <basalt/server/runtime/world/Conduits.hpp>

#include <cstdlib>

namespace basalt::server::runtime::world::conduits {

std::vector<Position> frame(const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& isWater,
                            const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& isFrame,
                            const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    for (std::int32_t ox = -1; ox <= 1; ++ox) {
        for (std::int32_t oy = -1; oy <= 1; ++oy) {
            for (std::int32_t oz = -1; oz <= 1; ++oz) {
                if (!isWater(x + ox, y + oy, z + oz)) {
                    return {};
                }
            }
        }
    }
    std::vector<Position> blocks;
    for (std::int32_t ox = -2; ox <= 2; ++ox) {
        for (std::int32_t oy = -2; oy <= 2; ++oy) {
            for (std::int32_t oz = -2; oz <= 2; ++oz) {
                const std::int32_t ax = std::abs(ox);
                const std::int32_t ay = std::abs(oy);
                const std::int32_t az = std::abs(oz);
                // The three rings of the frame: the outer square in each plane through the middle.
                const bool outside = ax > 1 || ay > 1 || az > 1;
                const bool ring = (ox == 0 && (ay == 2 || az == 2)) || (oy == 0 && (ax == 2 || az == 2)) ||
                                  (oz == 0 && (ax == 2 || ay == 2));
                if (outside && ring && isFrame(x + ox, y + oy, z + oz)) {
                    blocks.push_back({x + ox, y + oy, z + oz});
                }
            }
        }
    }
    return blocks;
}

bool closerThan(const Position& from, const Position& to, const double distance) {
    const auto dx = static_cast<double>(from[0] - to[0]);
    const auto dy = static_cast<double>(from[1] - to[1]);
    const auto dz = static_cast<double>(from[2] - to[2]);
    return (dx * dx) + (dy * dy) + (dz * dz) < distance * distance;
}

} // namespace basalt::server::runtime::world::conduits

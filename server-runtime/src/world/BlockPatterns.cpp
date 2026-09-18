#include <basalt/server/runtime/world/BlockPatterns.hpp>

#include <optional>

namespace basalt::server::runtime::world::blockPatterns {
namespace {

using registry::BlockFace;

constexpr std::array<BlockFace, 6> kDirections{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                               BlockFace::South, BlockFace::West, BlockFace::East};

struct Vec {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

[[nodiscard]] constexpr Vec vectorOf(const BlockFace face) noexcept {
    return Vec{registry::stepX(face), registry::stepY(face), registry::stepZ(face)};
}

[[nodiscard]] constexpr Vec cross(const Vec& a, const Vec& b) noexcept {
    return Vec{(a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x)};
}

} // namespace

Cell translateAndRotate(const Cell& origin, const BlockFace forwards, const BlockFace up, const std::int32_t right,
                        const std::int32_t down, const std::int32_t ahead) {
    const Vec forwardsVector = vectorOf(forwards);
    const Vec upVector = vectorOf(up);
    const Vec rightVector = cross(forwardsVector, upVector);
    return {origin[0] + (upVector.x * -down) + (rightVector.x * right) + (forwardsVector.x * ahead),
            origin[1] + (upVector.y * -down) + (rightVector.y * right) + (forwardsVector.y * ahead),
            origin[2] + (upVector.z * -down) + (rightVector.z * right) + (forwardsVector.z * ahead)};
}

std::optional<Found> find(const Cell& at, const std::int32_t size, const Matches& matches) {
    for (std::int32_t dz = 0; dz < size; ++dz) {
        for (std::int32_t dy = 0; dy < size; ++dy) {
            for (std::int32_t dx = 0; dx < size; ++dx) {
                const Cell origin{at[0] + dx, at[1] + dy, at[2] + dz};
                for (const BlockFace forwards : kDirections) {
                    for (const BlockFace up : kDirections) {
                        if (up == forwards || up == registry::opposite(forwards)) {
                            continue;
                        }
                        if (matches(origin, forwards, up)) {
                            return Found{.origin = origin, .forwards = forwards, .up = up};
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::blockPatterns

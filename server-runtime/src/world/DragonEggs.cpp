#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DragonEggs.hpp>

namespace basalt::server::runtime::world::dragonEggs {
namespace {

constexpr std::int32_t kAttempts = 1000;
constexpr std::int32_t kReachX = 16;
constexpr std::int32_t kReachY = 8;
constexpr std::int32_t kReachZ = 16;

[[nodiscard]] std::int32_t pack(std::int32_t dx, std::int32_t dy, std::int32_t dz) {
    return (((dx + kReachX) & 255) << 16) | (((dy + kReachY) & 255) << 8) | ((dz + kReachZ) & 255);
}

} // namespace

std::optional<Moved> teleport(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t minY,
                              std::int32_t maxY, random::RandomSource& random,
                              const std::function<bool(std::int32_t x, std::int32_t z)>& withinBorder) {
    const std::optional<std::int32_t> egg = world.blockState(x, y, z);
    if (!egg.has_value()) {
        return std::nullopt;
    }
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t attempt = 0; attempt < kAttempts; ++attempt) {
        // parity: the three offsets are each a difference of two draws, in this order.
        const std::int32_t dx = random.nextInt(16) - random.nextInt(16);
        const std::int32_t dy = random.nextInt(8) - random.nextInt(8);
        const std::int32_t dz = random.nextInt(16) - random.nextInt(16);
        const std::int32_t tx = x + dx;
        const std::int32_t ty = y + dy;
        const std::int32_t tz = z + dz;
        if (ty < minY || ty >= maxY || (withinBorder && !withinBorder(tx, tz))) {
            continue;
        }
        const std::optional<std::int32_t> at = world.blockState(tx, ty, tz);
        const std::optional<std::int32_t> below = world.blockState(tx, ty - 1, tz);
        if (!at.has_value() || !registry::blocks::isAir(*at) || !below.has_value() || registry::blocks::isAir(*below)) {
            continue;
        }
        if (!world.setBlockState(tx, ty, tz, *egg) || !world.setBlockState(x, y, z, air)) {
            return std::nullopt;
        }
        return Moved{.toX = tx, .toY = ty, .toZ = tz, .stateId = *egg, .packedDifference = pack(dx, dy, dz)};
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::dragonEggs

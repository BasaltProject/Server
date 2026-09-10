#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/Rails.hpp>

#include <cmath>
#include <cstdint>
#include <set>

namespace basalt::server::runtime::world::explosions {
namespace {

constexpr std::int32_t kRays = 16;
/// parity: the shell of a 16-cubed grid, so the last index on each axis is fifteen.
constexpr float kLastRay = 15.0F;
constexpr float kStep = 0.3F;
constexpr float kStepCost = 0.22500001F;
constexpr float kResistanceBias = 0.3F;

/// parity: `Block.getExplosionResistance`, and bedrock's own refusal.
[[nodiscard]] float resistanceOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return 0.0F;
    }
    const registry::BlockHardness* hardness = registry::blocks::hardnessOf(*block);
    return hardness == nullptr ? 0.0F : hardness->explosionResistance;
}

[[nodiscard]] bool canBreak(std::int32_t stateId) {
    if (stateId == registry::blocks::air().stateId) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const registry::BlockHardness* hardness = registry::blocks::hardnessOf(*block);
    return hardness == nullptr || !hardness->isUnbreakable();
}

[[nodiscard]] std::int64_t packed(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(z) & 0x3FFFFFF) << 12) |
           (static_cast<std::int64_t>(y) & 0xFFF);
}

[[nodiscard]] std::int32_t unpackX(std::int64_t value) { return static_cast<std::int32_t>(value >> 38); }

[[nodiscard]] std::int32_t unpackZ(std::int64_t value) { return static_cast<std::int32_t>(value << 26 >> 38); }

[[nodiscard]] std::int32_t unpackY(std::int64_t value) { return static_cast<std::int32_t>(value << 52 >> 52); }

} // namespace

bool sparesRails(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const std::int32_t stateId) {
    if (rails::isRail(stateId)) {
        return true;
    }
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    return above.has_value() && rails::isRail(*above);
}

std::vector<Broken> explode(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y, double z,
                            float power, random::RandomSource& random, const Spares& spares) {
    // A set rather than a hash: the order the blocks come out in is the order they are broken, and a
    // stable one keeps what the caller sends to clients stable too.
    std::set<std::int64_t> hit;
    for (std::int32_t rayX = 0; rayX < kRays; ++rayX) {
        for (std::int32_t rayY = 0; rayY < kRays; ++rayY) {
            for (std::int32_t rayZ = 0; rayZ < kRays; ++rayZ) {
                const bool onShell =
                    rayX == 0 || rayX == kRays - 1 || rayY == 0 || rayY == kRays - 1 || rayZ == 0 || rayZ == kRays - 1;
                if (!onShell) {
                    continue;
                }
                // parity: the three components are worked out in float and only then widened, which
                // is what `xx / 15.0F * 2.0F - 1.0F` does. In double they come out a few ulps apart,
                // and a ray that starts a few ulps off crosses a block boundary somewhere else.
                double dx = static_cast<double>((static_cast<float>(rayX) / kLastRay * 2.0F) - 1.0F);
                double dy = static_cast<double>((static_cast<float>(rayY) / kLastRay * 2.0F) - 1.0F);
                double dz = static_cast<double>((static_cast<float>(rayZ) / kLastRay * 2.0F) - 1.0F);
                const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
                dx /= length;
                dy /= length;
                dz /= length;
                float remaining = power * (0.7F + (random.nextFloat() * 0.6F));
                double px = x;
                double py = y;
                double pz = z;
                while (remaining > 0.0F) {
                    const auto blockX = static_cast<std::int32_t>(std::floor(px));
                    const auto blockY = static_cast<std::int32_t>(std::floor(py));
                    const auto blockZ = static_cast<std::int32_t>(std::floor(pz));
                    if (blockY < minY || blockY > maxY) {
                        break;
                    }
                    const std::optional<std::int32_t> stateId = world.blockState(blockX, blockY, blockZ);
                    if (!stateId.has_value()) {
                        break;
                    }
                    const bool spared = spares && spares(world, blockX, blockY, blockZ, *stateId);
                    if (*stateId != registry::blocks::air().stateId) {
                        remaining -= ((spared ? 0.0F : resistanceOf(*stateId)) + kResistanceBias) * kStep;
                    }
                    if (remaining > 0.0F && !spared && canBreak(*stateId)) {
                        hit.insert(packed(blockX, blockY, blockZ));
                    }
                    // parity: `xp += xd * 0.3F`, where the float step is widened for the sum.
                    px += dx * static_cast<double>(kStep);
                    py += dy * static_cast<double>(kStep);
                    pz += dz * static_cast<double>(kStep);
                    remaining -= kStepCost;
                }
            }
        }
    }
    std::vector<Broken> broken;
    for (const std::int64_t position : hit) {
        const std::int32_t bx = unpackX(position);
        const std::int32_t by = unpackY(position);
        const std::int32_t bz = unpackZ(position);
        const std::optional<std::int32_t> stateId = world.blockState(bx, by, bz);
        if (!stateId.has_value() || *stateId == registry::blocks::air().stateId) {
            continue;
        }
        if (!world.setBlockState(bx, by, bz, registry::blocks::air().stateId)) {
            continue;
        }
        broken.push_back(Broken{.x = bx, .y = by, .z = bz, .stateId = *stateId});
    }
    return broken;
}

float damageAt(double distance, float power, float radius) {
    if (distance >= static_cast<double>(radius)) {
        return 0.0F;
    }
    const double nearness = 1.0 - (distance / static_cast<double>(radius));
    return static_cast<float>(((((nearness * nearness) + nearness) / 2.0) * 7.0 * static_cast<double>(power)) + 1.0);
}

} // namespace basalt::server::runtime::world::explosions

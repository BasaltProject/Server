#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world::stuckSpeed {
namespace {

/// parity: `checkInsideBlocks` deflates the box at the target before asking who it touches.
constexpr double kSlack = 1.0e-5;

/// parity: `WebBlock.entityInside`. The weaving effect widens it, and nothing here has effects.
constexpr Multiplier kCobweb{.x = 0.25, .y = 0.05, .z = 0.25};

/// parity: `PowderSnowBlock.entityInside`, which catches anything that is **not** alive with no
/// further questions. Its vertical share is over one, so a fall through it is a shove down.
constexpr Multiplier kPowderSnow{.x = 0.9, .y = 1.5, .z = 0.9};

[[nodiscard]] std::optional<Multiplier> multiplierOf(std::string_view path) {
    if (path == "cobweb") {
        return kCobweb;
    }
    if (path == "powder_snow") {
        return kPowderSnow;
    }
    return std::nullopt;
}

} // namespace

bool holds(const Multiplier& multiplier) {
    const double lengthSquared =
        (multiplier.x * multiplier.x) + (multiplier.y * multiplier.y) + (multiplier.z * multiplier.z);
    return lengthSquared > entity::entityCollision::kEpsilon;
}

std::optional<Multiplier> caughtBy(const entity::entityCollision::BlockAt& blockAt, const entity::Aabb& box,
                                   const double dx, const double dy, const double dz) {
    const entity::Aabb swept = box.inflate(-kSlack).expandTowards(-dx, -dy, -dz);
    std::optional<Multiplier> found;
    for (std::int32_t x = wire::common::mth::floor(swept.minX); x <= wire::common::mth::floor(swept.maxX); ++x) {
        for (std::int32_t y = wire::common::mth::floor(swept.minY); y <= wire::common::mth::floor(swept.maxY); ++y) {
            for (std::int32_t z = wire::common::mth::floor(swept.minZ); z <= wire::common::mth::floor(swept.maxZ);
                 ++z) {
                const std::optional<std::int32_t> stateId = blockAt(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
                if (block == nullptr) {
                    continue;
                }
                // parity: every block the walk visits calls `makeStuckInBlock` in turn, so the last
                // one wins. Two of these in one box is a corner nobody builds.
                if (const std::optional<Multiplier> caught = multiplierOf(block->path()); caught.has_value()) {
                    found = caught;
                }
            }
        }
    }
    return found;
}

} // namespace basalt::server::runtime::world::stuckSpeed

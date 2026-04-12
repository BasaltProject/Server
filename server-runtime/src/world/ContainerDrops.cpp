#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::world::containerDrops {
namespace {

/// parity: `EntityTypes.ITEM`'s width, and the spread of the throw.
constexpr double kItemWidth = 0.25;
constexpr double kSpread = 0.11485000171139836;

} // namespace

void dropItemStack(WorldState& world, double x, double y, double z, server::world::loot::ItemStack stack,
                   random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId) {
    constexpr double kCentreRange = 1.0 - kItemWidth;
    const double xo = std::floor(x) + (random.nextDouble() * kCentreRange) + (kItemWidth / 2.0);
    const double yo = std::floor(y) + (random.nextDouble() * kCentreRange);
    const double zo = std::floor(z) + (random.nextDouble() * kCentreRange) + (kItemWidth / 2.0);
    while (stack.count > 0) {
        const std::int32_t handful = std::min(stack.count, random.nextInt(21) + 10);
        stack.count -= handful;
        DroppedItem dropped{.entityId = allocateEntityId(),
                            .uuid = wire::common::Uuid::random(),
                            .item = stack.item,
                            .count = handful,
                            .x = xo,
                            .y = yo,
                            .z = zo,
                            .components = stack.components};
        dropped.velocityX = random.triangle(0.0, kSpread);
        dropped.velocityY = random.triangle(0.2, kSpread);
        dropped.velocityZ = random.triangle(0.0, kSpread);
        static_cast<void>(world.spawnItem(std::move(dropped)));
    }
}

} // namespace basalt::server::runtime::world::containerDrops

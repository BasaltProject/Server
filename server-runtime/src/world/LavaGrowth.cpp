#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/LavaGrowth.hpp>

namespace basalt::server::runtime::world::lavaGrowth {
namespace {

[[nodiscard]] std::int32_t igniteOddsAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return registry::fireOdds::igniteOdds(world.blockState(x, y, z).value_or(0));
}

[[nodiscard]] bool flammableAround(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return igniteOddsAt(world, x - 1, y, z) > 0 || igniteOddsAt(world, x + 1, y, z) > 0 ||
           igniteOddsAt(world, x, y, z - 1) > 0 || igniteOddsAt(world, x, y, z + 1) > 0 ||
           igniteOddsAt(world, x, y - 1, z) > 0 || igniteOddsAt(world, x, y + 1, z) > 0;
}

void light(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const registry::BlockState* fire = registry::blocks::named("fire");
    if (fire == nullptr) {
        return;
    }
    growthSupport::set(world, x, y, z, fire->stateId, changes);
}

} // namespace

void lava(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
          std::vector<BlockChange>& changes) {
    const std::int32_t passes = random.nextInt(3);
    if (passes > 0) {
        // parity: the walk upwards, which stops at the first block that would stand in the way.
        std::int32_t testX = x;
        std::int32_t testY = y;
        std::int32_t testZ = z;
        for (std::int32_t pass = 0; pass < passes; ++pass) {
            testX += random.nextInt(3) - 1;
            testY += 1;
            testZ += random.nextInt(3) - 1;
            const std::optional<std::int32_t> here = world.blockState(testX, testY, testZ);
            if (!here.has_value()) {
                return;
            }
            if (*here == registry::blocks::air().stateId) {
                if (flammableAround(world, testX, testY, testZ)) {
                    light(world, testX, testY, testZ, changes);
                    return;
                }
            } else if (registry::blockShapes::blocksMotion(*here)) {
                return;
            }
        }
        return;
    }

    // parity: three tries at the air directly over something that will burn.
    for (std::int32_t pass = 0; pass < 3; ++pass) {
        const std::int32_t testX = x + random.nextInt(3) - 1;
        const std::int32_t testZ = z + random.nextInt(3) - 1;
        if (world.blockState(testX, y + 1, testZ) != registry::blocks::air().stateId) {
            continue;
        }
        if (igniteOddsAt(world, testX, y, testZ) <= 0) {
            continue;
        }
        light(world, testX, y + 1, testZ, changes);
    }
}

} // namespace basalt::server::runtime::world::lavaGrowth

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockTraits.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/LavaGrowth.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::lavaGrowth {
namespace {

// parity: `LavaFluid.isFlammable` asks `ignitedByLava`, not the fire odds (coal blocks burn, leaves do not).
[[nodiscard]] bool ignitedAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && registry::blockTraits::ignitedByLava(*state);
}

[[nodiscard]] bool flammableAround(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return ignitedAt(world, x - 1, y, z) || ignitedAt(world, x + 1, y, z) || ignitedAt(world, x, y, z - 1) ||
           ignitedAt(world, x, y, z + 1) || ignitedAt(world, x, y - 1, z) || ignitedAt(world, x, y + 1, z);
}

// parity: `LavaFluid.randomTick` reads `BaseFireBlock.getState` at the tested block, not where the fire lands.
void light(WorldState& world, std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ, std::int32_t x,
           std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> fire = fires::newFireAt(world, fromX, fromY, fromZ);
    if (fire.has_value()) {
        growthSupport::set(world, x, y, z, *fire, changes);
    }
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
                    light(world, testX, testY, testZ, testX, testY, testZ, changes);
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
        if (!world.blockState(testX, y, testZ).has_value()) {
            return;
        }
        if (world.blockState(testX, y + 1, testZ) != registry::blocks::air().stateId ||
            !ignitedAt(world, testX, y, testZ)) {
            continue;
        }
        light(world, testX, y, testZ, testX, y + 1, testZ, changes);
    }
}

} // namespace basalt::server::runtime::world::lavaGrowth

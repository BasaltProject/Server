#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/MushroomGrowth.hpp>

namespace basalt::server::runtime::world::mushroomGrowth {
namespace {

/// Whether a mushroom may sit here: empty, and something it can survive on.
[[nodiscard]] bool fits(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                        const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen, bool hasSkylight) {
    if (world.blockState(x, y, z) != registry::blocks::air().stateId) {
        return false;
    }
    return blockSurvival::isSupported(world, x, y, z, stateId, scaffoldingHasFallen, hasSkylight);
}

} // namespace

void mushroom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
              random::RandomSource& random, std::vector<BlockChange>& changes,
              const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen, bool hasSkylight) {
    if (random.nextInt(kOdds) != 0) {
        return;
    }
    // parity: five of its own kind in reach and it stops, which is what keeps a cave from filling.
    std::int32_t room = kCrowd;
    for (std::int32_t dx = -4; dx <= 4; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -4; dz <= 4; ++dz) {
                if (world.blockState(x + dx, y + dy, z + dz) != stateId) {
                    continue;
                }
                if (--room <= 0) {
                    return;
                }
            }
        }
    }

    // parity: four hops, each from the last place that would hold a mushroom.
    // The first hop is drawn exactly like the three that follow it; the only thing that makes it
    // special is that it starts from the mushroom itself.
    std::int32_t hopX = x + random.nextInt(3) - 1;
    std::int32_t hopY = y + random.nextInt(2) - random.nextInt(2);
    std::int32_t hopZ = z + random.nextInt(3) - 1;
    std::int32_t fromX = x;
    std::int32_t fromY = y;
    std::int32_t fromZ = z;
    for (std::int32_t hop = 0; hop < 4; ++hop) {
        if (fits(world, hopX, hopY, hopZ, stateId, scaffoldingHasFallen, hasSkylight)) {
            fromX = hopX;
            fromY = hopY;
            fromZ = hopZ;
        }
        hopX = fromX + random.nextInt(3) - 1;
        hopY = fromY + random.nextInt(2) - random.nextInt(2);
        hopZ = fromZ + random.nextInt(3) - 1;
    }
    if (fits(world, hopX, hopY, hopZ, stateId, scaffoldingHasFallen, hasSkylight)) {
        growthSupport::set(world, hopX, hopY, hopZ, stateId, changes);
    }
}

} // namespace basalt::server::runtime::world::mushroomGrowth

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <string>

namespace basalt::server::runtime::world::lightning {
namespace {

/// parity: the tag `lightning_rods`, which is **eight** blocks and not one: the rod oxidises like
/// any other copper, and each stage is its own block. Every one of the eight ends in `lightning_rod`,
/// and nothing else does, so the name is the whole test.
[[nodiscard]] bool isRod(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path().ends_with("lightning_rod");
}

void powerRod(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isRod(*stateId)) {
        return;
    }
    // The rod's **own** block, not the plain one: writing the plain one here would turn a weathered
    // rod into a fresh one every time it was struck.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"powered", "true"}}, *stateId);
    if (!turned.has_value()) {
        return;
    }
    if (world.setBlockState(x, y, z, *turned)) {
        // parity: `LightningRodBlock.onLightningStrike` ends in `levelEvent(3002, pos, axis)`, the
        // sparks running along the rod, and the axis is the ordinal of its facing's own axis.
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*turned);
        const auto facing = properties.find("facing");
        const std::int32_t axis = facing == properties.end()                                 ? 1
                                  : (facing->second == "up" || facing->second == "down")     ? 1
                                  : (facing->second == "north" || facing->second == "south") ? 2
                                                                                             : 0;
        changes.push_back(BlockChange{
            .x = x,
            .y = y,
            .z = z,
            .stateId = *turned,
            .effects = {blockEffect::Effect{.type = blockEffect::Effect::kLightningRodSparks, .data = axis}}});
    }
    world.scheduleBlockTick(x, y, z, *block, kRodTicks);
}

/// parity: `findLightningRod`, which takes the closest rod whose tip is the top of its own column.
///
/// Vanilla searches its point-of-interest index; here the loaded chunks around the target are walked
/// instead, which reaches the same rods without an index to keep.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> rodNear(WorldState& world, std::int32_t x, std::int32_t y,
                                                                 std::int32_t z) {
    std::optional<std::array<std::int32_t, 3>> best;
    std::int32_t bestDistance = std::numeric_limits<std::int32_t>::max();
    const std::int32_t radius = kRodRange >> 4;
    for (std::int32_t chunkX = (x >> 4) - radius; chunkX <= (x >> 4) + radius; ++chunkX) {
        for (std::int32_t chunkZ = (z >> 4) - radius; chunkZ <= (z >> 4) + radius; ++chunkZ) {
            const server::world::Chunk* chunk = world.chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (std::int32_t localX = 0; localX < 16; ++localX) {
                for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                    const std::int32_t top =
                        chunk->heightAt(localX, localZ, server::world::HeightmapType::MotionBlocking);
                    const std::int32_t rodX = (chunkX << 4) + localX;
                    const std::int32_t rodZ = (chunkZ << 4) + localZ;
                    const std::optional<std::int32_t> stateId = world.blockState(rodX, top - 1, rodZ);
                    if (!stateId.has_value() || !isRod(*stateId)) {
                        continue;
                    }
                    const std::int32_t distance =
                        ((rodX - x) * (rodX - x)) + ((top - y) * (top - y)) + ((rodZ - z) * (rodZ - z));
                    if (distance > kRodRange * kRodRange || distance >= bestDistance) {
                        continue;
                    }
                    bestDistance = distance;
                    best = std::array<std::int32_t, 3>{rodX, top, rodZ};
                }
            }
        }
    }
    return best;
}

} // namespace

const wire::common::Identifier& rod() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("lightning_rod");
    return instance;
}

const wire::common::Identifier& bolt() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("lightning_bolt");
    return instance;
}

std::optional<std::array<std::int32_t, 3>> targetFor(WorldState& world, std::int32_t x, std::int32_t z) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return std::nullopt;
    }
    const std::int32_t top = chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlocking);
    const std::optional<std::array<std::int32_t, 3>> nearby = rodNear(world, x, top, z);
    if (nearby.has_value()) {
        return nearby;
    }
    return std::array<std::int32_t, 3>{x, top, z};
}

void strike(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool lightFires,
            random::RandomSource& random, std::vector<BlockChange>& changes) {
    powerRod(world, x, y - 1, z, changes);
    powerRod(world, x, y, z, changes);
    if (!lightFires) {
        return;
    }
    if (fires::ignite(world, x, y, z, random)) {
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = world.blockState(x, y, z).value_or(0)});
    }
}

std::optional<BlockChange> releaseRod(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isRod(*stateId)) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto powered = properties.find("powered");
    if (powered == properties.end() || powered->second != "true") {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"powered", "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

bool shouldStrike(random::RandomSource& random) { return random.nextInt(kStrikeOdds) == 0; }

} // namespace basalt::server::runtime::world::lightning

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/SculkShriekers.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace basalt::server::runtime::world::sculkShriekers {
namespace {

[[nodiscard]] bool property(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found != properties.end() && found->second == "true";
}

[[nodiscard]] wire::nbt::NbtCompound* dataOf(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             bool create) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15); entity != nullptr) {
        return &entity->data();
    }
    if (!create) {
        return nullptr;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{
        x & 15, y, z & 15, wire::common::Identifier::vanilla("sculk_shrieker"), wire::nbt::NbtCompound{}});
    server::world::ChunkBlockEntity* made = chunk->blockEntityAt(x & 15, y, z & 15);
    return made == nullptr ? nullptr : &made->data();
}

} // namespace

bool isShrieker(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "sculk_shrieker";
}

bool isShrieking(std::int32_t stateId) { return isShrieker(stateId) && property(stateId, "shrieking"); }

bool canSummon(std::int32_t stateId) { return isShrieker(stateId) && property(stateId, "can_summon"); }

std::optional<std::int32_t> withShrieking(std::int32_t stateId, bool shrieking) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, {{"shrieking", shrieking ? "true" : "false"}}, stateId);
}

std::int32_t warningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const wire::nbt::NbtCompound* data = dataOf(world, x, y, z, false);
    return data == nullptr ? 0 : data->getInt(kWarningKey);
}

void setWarningLevel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
    if (wire::nbt::NbtCompound* data = dataOf(world, x, y, z, true); data != nullptr) {
        data->putInt(kWarningKey, level);
    }
}

std::optional<std::array<std::int32_t, 3>> spawnPosition(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, random::RandomSource& random,
                                                         std::int32_t& attempt) {
    for (; attempt < kSpawnAttempts;) {
        attempt++;
        const std::int32_t sx = x + random.nextIntBetweenInclusive(-kSpawnRangeXZ, kSpawnRangeXZ);
        const std::int32_t sz = z + random.nextIntBetweenInclusive(-kSpawnRangeXZ, kSpawnRangeXZ);
        std::int32_t searchY = y + kSpawnRangeY;
        std::optional<std::int32_t> above = world.blockState(sx, searchY, sz);
        // parity: `moveToPossibleSpawnPosition`, down from six above to six below.
        for (std::int32_t step = kSpawnRangeY; step >= -kSpawnRangeY && above.has_value(); --step) {
            searchY--;
            const std::optional<std::int32_t> current = world.blockState(sx, searchY, sz);
            if (!current.has_value()) {
                break;
            }
            // parity: `ON_TOP_OF_COLLIDER`: nothing to bump into above, a full top face below.
            if (registry::blockShapes::isEmptyShape(*above) &&
                registry::blockShapes::isCollisionFaceFull(*current, registry::BlockFace::Up)) {
                return std::array<std::int32_t, 3>{sx, searchY + 1, sz};
            }
            above = current;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::sculkShriekers

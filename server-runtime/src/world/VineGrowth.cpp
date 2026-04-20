#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/VineGrowth.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::vineGrowth {
namespace {

/// parity: `VineBlock.isAcceptableNeighbour` over `MultifaceBlock.canAttachTo`.
[[nodiscard]] bool clingsTo(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            std::string_view facing) {
    const std::optional<std::int32_t> neighbour = world.blockState(x, y, z);
    if (!neighbour.has_value()) {
        return false;
    }
    registry::BlockFace face = registry::BlockFace::West;
    if (facing == "up") {
        face = registry::BlockFace::Down;
    } else if (facing == "north") {
        face = registry::BlockFace::South;
    } else if (facing == "south") {
        face = registry::BlockFace::North;
    } else if (facing == "west") {
        face = registry::BlockFace::East;
    }
    return registry::blockShapes::isFaceSturdy(*neighbour, face);
}

[[nodiscard]] bool hasRoom(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    std::int32_t room = kCrowd;
    for (std::int32_t dx = -4; dx <= 4; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -4; dz <= 4; ++dz) {
                const std::optional<std::int32_t> here = world.blockState(x + dx, y + dy, z + dz);
                if (!here.has_value()) {
                    continue;
                }
                const wire::common::Identifier* theirs = registry::blockStateDefinitions::blockOf(*here);
                if (theirs == nullptr || *theirs != *block) {
                    continue;
                }
                if (--room <= 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

void vine(WorldState& world, const GameRuleValues& gameRules, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (!gameRules.get(gameRules::kSpreadVines)) {
        return;
    }
    if (random.nextInt(kOdds) != 0) {
        return;
    }
    if (!hasRoom(world, x, y, z, stateId)) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (block == nullptr || chunk == nullptr) {
        return;
    }
    const std::string_view facing = growthSupport::kFaceNames[static_cast<std::size_t>(
        random.nextInt(static_cast<std::int32_t>(growthSupport::kFaceNames.size())))];

    if (facing == "up") {
        if (y >= chunk->verticalBounds().maxY()) {
            return;
        }
        if (propertyOf(stateId, "up") == "true") {
            return;
        }
        if (!clingsTo(world, x, y + 1, z, "up")) {
            return;
        }
        const std::optional<std::int32_t> climbed =
            registry::blockStateDefinitions::stateIdOf(*block, {{"up", "true"}}, stateId);
        if (!climbed.has_value()) {
            return;
        }
        growthSupport::set(world, x, y, z, *climbed, changes);
        return;
    }
    // parity: a vine never takes its own underside. Falling through would ask the block at its own.
    if (facing == "down") {
        return;
    }
    const std::string face{facing};
    if (propertyOf(stateId, face) == "true") {
        return;
    }
    const std::int32_t sideX = x + growthSupport::offsetX(facing);
    const std::int32_t sideZ = z + growthSupport::offsetZ(facing);
    const std::optional<std::int32_t> beside = world.blockState(sideX, y, sideZ);
    if (!beside.has_value() || *beside == registry::blocks::air().stateId) {
        return;
    }
    if (!clingsTo(world, sideX, y, sideZ, facing)) {
        return;
    }
    const std::optional<std::int32_t> held =
        registry::blockStateDefinitions::stateIdOf(*block, {{face, "true"}}, stateId);
    if (!held.has_value()) {
        return;
    }
    growthSupport::set(world, x, y, z, *held, changes);
}

} // namespace basalt::server::runtime::world::vineGrowth

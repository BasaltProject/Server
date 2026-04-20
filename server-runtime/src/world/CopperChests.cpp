#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/CopperChests.hpp>

#include <array>
#include <map>

namespace basalt::server::runtime::world::copperChests {

namespace {

using registry::BlockFace;

[[nodiscard]] std::string clockwise(const std::string& facing) {
    if (facing == "north") {
        return "east";
    }
    if (facing == "east") {
        return "south";
    }
    return facing == "south" ? "west" : "north";
}

[[nodiscard]] std::string counterClockwise(const std::string& facing) {
    return clockwise(clockwise(clockwise(facing)));
}

[[nodiscard]] std::optional<BlockFace> faceNamed(const std::string& name) {
    if (name == "north") {
        return BlockFace::North;
    }
    if (name == "south") {
        return BlockFace::South;
    }
    if (name == "west") {
        return BlockFace::West;
    }
    if (name == "east") {
        return BlockFace::East;
    }
    return std::nullopt;
}

[[nodiscard]] std::string propertyOf(const std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::int32_t stateAt(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                   const std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

/// parity: `ChestBlock.candidatePartnerFacing`.
[[nodiscard]] std::string partnerFacing(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                        const std::int32_t z, const std::string& side) {
    const std::optional<BlockFace> face = faceNamed(side);
    if (!face.has_value()) {
        return {};
    }
    const std::int32_t neighbour = stateAt(world, x + registry::stepX(*face), y, z + registry::stepZ(*face));
    if (!isCopperChest(pathOf(neighbour)) || propertyOf(neighbour, "type") != "single") {
        return {};
    }
    return propertyOf(neighbour, "facing");
}

/// parity: `ChestBlock.getConnectedDirection`.
[[nodiscard]] std::string connectedSide(const std::string& facing, const std::string& type) {
    return type == "left" ? clockwise(facing) : counterClockwise(facing);
}

} // namespace

bool isCopperChest(const std::string_view path) { return path.ends_with("copper_chest"); }

std::int32_t oxidationOf(const std::string_view path) {
    if (path.find("exposed") != std::string_view::npos) {
        return 1;
    }
    if (path.find("weathered") != std::string_view::npos) {
        return 2;
    }
    return path.find("oxidized") != std::string_view::npos ? 3 : 0;
}

std::string chestForBlock(const std::string_view copperBlockPath) {
    // parity: `BlockItemIds.COPPER_BLOCK_SPECIAL_NAMES`: only the unaffected one is called.
    static constexpr std::array<std::string_view, 8> kBodies{
        "copper_block",       "exposed_copper",       "weathered_copper",       "oxidized_copper",
        "waxed_copper_block", "waxed_exposed_copper", "waxed_weathered_copper", "waxed_oxidized_copper"};
    static constexpr std::array<std::string_view, 8> kChests{"copper_chest",
                                                             "exposed_copper_chest",
                                                             "weathered_copper_chest",
                                                             "oxidized_copper_chest",
                                                             "waxed_copper_chest",
                                                             "waxed_exposed_copper_chest",
                                                             "waxed_weathered_copper_chest",
                                                             "waxed_oxidized_copper_chest"};
    for (std::size_t index = 0; index < kBodies.size(); ++index) {
        if (kBodies[index] == copperBlockPath) {
            return std::string{kChests[index]};
        }
    }
    // parity: `getOrDefault(copperBlock, Blocks.COPPER_CHEST.weathering().unaffected())`.
    return "copper_chest";
}

std::optional<std::int32_t> placedState(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                        const std::int32_t z, const std::string_view chestPath,
                                        const std::string& facing) {
    // parity: `getChestType`.
    std::string type = "single";
    if (facing == partnerFacing(world, x, y, z, clockwise(facing))) {
        type = "left";
    } else if (facing == partnerFacing(world, x, y, z, counterClockwise(facing))) {
        type = "right";
    }
    std::string chest{chestPath};
    if (type != "single") {
        // parity: `getLeastOxidizedChestOfConnectedBlocks`.
        const std::optional<BlockFace> face = faceNamed(connectedSide(facing, type));
        if (face.has_value()) {
            const std::int32_t joined = stateAt(world, x + registry::stepX(*face), y, z + registry::stepZ(*face));
            const std::string other = pathOf(joined);
            if (isCopperChest(other) && oxidationOf(other) < oxidationOf(chest)) {
                chest = other;
            }
        }
    }
    return registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla(chest),
                                                      {{"facing", facing}, {"type", type}, {"waterlogged", "false"}},
                                                      std::nullopt);
}

} // namespace basalt::server::runtime::world::copperChests

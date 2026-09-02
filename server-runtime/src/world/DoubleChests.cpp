#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace basalt::server::runtime::world::doubleChests {
namespace {

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

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

[[nodiscard]] std::int32_t stepX(const std::string& facing) {
    if (facing == "west") {
        return -1;
    }
    return facing == "east" ? 1 : 0;
}

[[nodiscard]] std::int32_t stepZ(const std::string& facing) {
    if (facing == "north") {
        return -1;
    }
    return facing == "south" ? 1 : 0;
}

} // namespace

std::optional<std::pair<Half, Half>> halves(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr ||
        std::ranges::find(reshapes::chestBlocks(), block->path()) == reshapes::chestBlocks().end()) {
        return std::nullopt;
    }
    const std::string type = propertyOf(*stateId, "type");
    const std::string facing = propertyOf(*stateId, "facing");
    if (type.empty() || type == "single" || facing.empty()) {
        return std::nullopt;
    }
    // parity: `getConnectedDirection`: a left half looks clockwise for its partner, a right one
    // counter-clockwise.
    const std::string towards = type == "left" ? clockwise(facing) : counterClockwise(facing);
    const Half partner{x + stepX(towards), y, z + stepZ(towards)};
    const std::optional<std::int32_t> partnerState = world.blockState(partner[0], partner[1], partner[2]);
    if (!partnerState.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* partnerBlock = registry::blockStateDefinitions::blockOf(*partnerState);
    if (partnerBlock == nullptr || *partnerBlock != *block || propertyOf(*partnerState, "facing") != facing ||
        propertyOf(*partnerState, "type") != (type == "left" ? "right" : "left")) {
        return std::nullopt;
    }
    const Half self{x, y, z};
    return type == "right" ? std::pair{self, partner} : std::pair{partner, self};
}

} // namespace basalt::server::runtime::world::doubleChests

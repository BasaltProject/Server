#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>

#include <charconv>
#include <map>
#include <string>

namespace basalt::server::runtime::world::respawnAnchors {
namespace {

/// parity: `RespawnAnchorBlock.canBeCharged`, which is `charges < 4`.
constexpr std::int32_t kMaxCharges = 4;

[[nodiscard]] std::int32_t chargesOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("charges");
    if (found == properties.end()) {
        return 0;
    }
    std::int32_t value = 0;
    const char* first = found->second.data();
    static_cast<void>(std::from_chars(first, first + found->second.size(), value));
    return value;
}

} // namespace

bool isAnchor(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "respawn_anchor";
}

std::optional<BlockChange> charge(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isAnchor(*stateId)) {
        return std::nullopt;
    }
    const std::int32_t charges = chargesOf(*stateId);
    if (charges >= kMaxCharges) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    const std::optional<std::int32_t> charged =
        registry::blockStateDefinitions::stateIdOf(*block, {{"charges", std::to_string(charges + 1)}}, *stateId);
    if (!charged.has_value() || !world.setBlockState(x, y, z, *charged)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *charged};
}

} // namespace basalt::server::runtime::world::respawnAnchors

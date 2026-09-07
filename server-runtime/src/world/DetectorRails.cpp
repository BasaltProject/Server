#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::detectorRails {
namespace {

constexpr std::array kMinecarts = {std::string_view{"minecart"},
                                   std::string_view{"chest_minecart"},
                                   std::string_view{"furnace_minecart"},
                                   std::string_view{"tnt_minecart"},
                                   std::string_view{"hopper_minecart"},
                                   std::string_view{"spawner_minecart"},
                                   std::string_view{"command_block_minecart"}};

[[nodiscard]] const wire::common::Identifier& railBlock() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("detector_rail");
    return instance;
}

} // namespace

bool isMinecart(const wire::common::Identifier& type) {
    return std::ranges::find(kMinecarts, type.path()) != kMinecarts.end();
}

std::optional<BlockChange> check(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::span<entity::ServerEntity* const> entities) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || *block != railBlock()) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto poweredAt = properties.find("powered");
    const bool powered = poweredAt != properties.end() && poweredAt->second == "true";

    // parity: `checkPressed`'s own box, which reaches a block below the rail and a block above it.
    const bool occupied = std::ranges::any_of(entities, [x, y, z](const entity::ServerEntity* standing) {
        return standing != nullptr && !standing->removed && isMinecart(standing->type()) && standing->x >= x &&
               standing->x <= x + 1 && standing->z >= z && standing->z <= z + 1 && standing->y >= y - 1 &&
               standing->y <= y + 1;
    });
    if (occupied) {
        world.scheduleBlockTick(x, y, z, railBlock(), kRecheckTicks);
    }
    if (occupied == powered) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"powered", occupied ? "true" : "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

} // namespace basalt::server::runtime::world::detectorRails

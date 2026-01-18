#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::detectorRails {
namespace {

/// parity: `getSearchBB`'s own fifth of a block.
constexpr double kSearchShrink = 0.2;

/// parity: `EntityTypes.MINECART.sized(0.98F, 0.7F)`, its height; the half width is public because
/// the caller needs it to know which rails a cart is touching.
constexpr double kCartHeight = 0.7;

} // namespace

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

    // parity: `DetectorRailBlock.getSearchBB`, the rail's own square shrunk a fifth of a block on
    // each horizontal side and off the top, against the **cart's own box** and not its middle. A
    // cart is on the rail a good quarter of a block before its centre crosses the edge: measured,
    // the jar fires at x 4.84 for a rail at 5 and going by the middle fired at 5.12.
    const double minX = x + kSearchShrink;
    const double maxX = x + 1 - kSearchShrink;
    const double minY = y;
    const double maxY = y + 1 - kSearchShrink;
    const double minZ = z + kSearchShrink;
    const double maxZ = z + 1 - kSearchShrink;
    const bool occupied =
        std::ranges::any_of(entities, [minX, maxX, minY, maxY, minZ, maxZ](const entity::ServerEntity* standing) {
            if (standing == nullptr || standing->removed || !isMinecart(standing->type())) {
                return false;
            }
            return standing->x + kCartHalfWidth > minX && standing->x - kCartHalfWidth < maxX &&
                   standing->y + kCartHeight > minY && standing->y < maxY && standing->z + kCartHalfWidth > minZ &&
                   standing->z - kCartHalfWidth < maxZ;
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

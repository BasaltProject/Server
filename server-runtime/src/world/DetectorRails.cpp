#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world::detectorRails {
namespace {

/// parity: `getSearchBB`'s own fifth of a block.
constexpr double kSearchShrink = 0.2;

/// parity: `EntityTypes.MINECART.sized(0.98F, 0.7F)`, its height; the half width is public because
/// the caller needs it to know which rails a cart is touching.
constexpr double kCartHeight = 0.7;

/// parity: `Container.getMaxStackSize`, which every minecart leaves at the default.
constexpr std::int32_t kDefaultMaxStack = 64;

} // namespace

namespace {

constexpr std::array kMinecarts = {std::string_view{"minecart"},
                                   std::string_view{"chest_minecart"},
                                   std::string_view{"furnace_minecart"},
                                   std::string_view{"tnt_minecart"},
                                   std::string_view{"hopper_minecart"},
                                   std::string_view{"spawner_minecart"},
                                   std::string_view{"command_block_minecart"}};

/// parity: `DetectorRailBlock.getSearchBB`, the rail's own square shrunk a fifth of a block on each
/// horizontal side and off the top, against the **cart's own box** and not its middle. A cart is on
/// the rail a good quarter of a block before its centre crosses the edge: measured, the jar fires at
/// x 4.84 for a rail at 5 and going by the middle fired at 5.12.
[[nodiscard]] bool touches(const entity::ServerEntity& cart, const std::int32_t x, const std::int32_t y,
                           const std::int32_t z) {
    if (cart.removed) {
        return false;
    }
    return cart.x + kCartHalfWidth > x + kSearchShrink && cart.x - kCartHalfWidth < x + 1 - kSearchShrink &&
           cart.y + kCartHeight > y && cart.y < y + 1 - kSearchShrink && cart.z + kCartHalfWidth > z + kSearchShrink &&
           cart.z - kCartHalfWidth < z + 1 - kSearchShrink;
}

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

    const bool occupied = std::ranges::any_of(entities, [x, y, z](const entity::ServerEntity* standing) {
        return standing != nullptr && isMinecart(standing->type()) && touches(*standing, x, y, z);
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

std::int32_t comparatorSignal(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              std::span<entity::ServerEntity* const> entities) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return 0;
    }
    // parity: a rail that nothing is standing on answers nothing at all, whatever is parked nearby.
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto poweredAt = properties.find("powered");
    if (poweredAt == properties.end() || poweredAt->second != "true") {
        return 0;
    }
    // parity: `EntitySelector.CONTAINER_ENTITY_SELECTOR`, so only the carts that hold something, and
    // the first one found rather than the fullest.
    for (const entity::ServerEntity* cart : entities) {
        if (cart == nullptr || !touches(*cart, x, y, z)) {
            continue;
        }
        const std::optional<std::int32_t> size = containers::sizeOf(cart->type().path());
        if (!size.has_value() || !cart->sourceNbt.has_value()) {
            continue;
        }
        const blockInventory::Slots slots = blockInventory::read(*cart->sourceNbt, *size);
        std::vector<std::pair<std::int32_t, std::int32_t>> counts;
        counts.reserve(slots.size());
        for (const std::optional<server::world::loot::ItemStack>& slot : slots) {
            counts.emplace_back(slot.has_value() ? slot->count : 0, kDefaultMaxStack);
        }
        return redstone::containerSignal(counts, *size);
    }
    return 0;
}

} // namespace basalt::server::runtime::world::detectorRails

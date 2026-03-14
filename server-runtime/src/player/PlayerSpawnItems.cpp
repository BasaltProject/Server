#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: `MinecartItem.useOn`, a sixteenth of a block above the rail.
constexpr double kMinecartRestHeight = 0.0625;

/// The square a click lands on, which is the block itself or the one beyond the face.
[[nodiscard]] std::array<std::int32_t, 3> beyond(const BlockHit& hit) {
    const wire::protocol::play::BlockPos at = wire::protocol::play::BlockPos{hit.x, hit.y, hit.z}.offset(hit.direction);
    return {at.x, at.y, at.z};
}

/// Puts one of these in the world and takes one off the stack that made it.
[[nodiscard]] bool spawnFromItem(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                                 const Identifier& type, const std::int32_t heldSlot,
                                 const storage::StoredItemStack& held, const double x, const double y, const double z) {
    const reg::EntityTypeEntry* entry = reg::entityTypes::of(type);
    if (entry == nullptr || context.registry == nullptr) {
        return false;
    }
    world.state().spawnEntity(std::make_unique<entity::ServerEntity>(
        context.registry->allocateEntityId(), wire::common::Uuid::random(), type, entry->protocolId, x, y, z));
    if (player.gamemode == Gamemode::Creative) {
        return true;
    }
    storage::StoredItemStack left = held;
    left.count = held.count - 1;
    player.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{left});
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
    return true;
}

} // namespace

bool placeMinecartFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                      const Identifier& item, const std::int32_t heldSlot, const storage::StoredItemStack& held,
                      const BlockHit& hit) {
    const std::optional<std::int32_t> rail = world.state().blockState(hit.x, hit.y, hit.z);
    if (!rail.has_value() || !world::rails::isRail(*rail)) {
        return false;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*rail);
    const auto shape = properties.find("shape");
    if (shape == properties.end()) {
        return false;
    }
    const double slope = shape->second.starts_with("ascending") ? 0.5 : 0.0;
    // The item and the entity share a name except for the plain cart, whose entity keeps the suffix.
    const std::string path{item.path()};
    const Identifier type =
        path == "minecart"
            ? item
            : Identifier::vanilla(path.substr(0, path.size() - std::string_view{"_minecart"}.size()) + "_minecart");
    if (!spawnFromItem(context, player, world, type, heldSlot, held, hit.x + 0.5, hit.y + kMinecartRestHeight + slope,
                       hit.z + 0.5)) {
        return false;
    }
    // parity: `MinecartItem` raises `ENTITY_PLACE` at the rail's own square, not at the cart.
    heardBySculk(context, world, "entity_place", hit.x, hit.y, hit.z);
    return true;
}

bool placeBoatFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const Identifier& item,
                  const std::int32_t heldSlot, const storage::StoredItemStack& held, const BlockHit& hit) {
    // parity: `getBoat` puts it at the hit itself, which is the face of the block clicked.
    const std::array<std::int32_t, 3> at = beyond(hit);
    if (!spawnFromItem(context, player, world, item, heldSlot, held, at[0] + 0.5, static_cast<double>(at[1]),
                       at[2] + 0.5)) {
        return false;
    }
    // parity: `BoatItem` raises `ENTITY_PLACE` at the exact point clicked, not at the square.
    world::vibrations::emit(world.state(), "entity_place", hit.x + static_cast<double>(hit.clickX),
                            hit.y + static_cast<double>(hit.clickY), hit.z + static_cast<double>(hit.clickZ),
                            world.gameTime());
    return true;
}

bool placeEndCrystalFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                        const std::int32_t heldSlot, const storage::StoredItemStack& held, const BlockHit& hit) {
    const std::optional<std::int32_t> clicked = world.state().blockState(hit.x, hit.y, hit.z);
    const reg::BlockState* obsidian = reg::blocks::named("obsidian");
    if (!clicked.has_value() ||
        (*clicked != reg::blocks::bedrock().stateId && (obsidian == nullptr || *clicked != obsidian->stateId))) {
        return false;
    }
    const std::int32_t x = hit.x;
    const std::int32_t y = hit.y + 1;
    const std::int32_t z = hit.z;
    const std::optional<std::int32_t> above = world.state().blockState(x, y, z);
    if (!above.has_value() || !reg::blocks::isAir(*above)) {
        return false;
    }
    // parity: the one-by-two box above the block has to hold no entity at all.
    const auto overlaps = [x, y, z](double ex, double ey, double ez, double width, double height) {
        const double half = width / 2.0;
        return ex + half > x && ex - half < x + 1.0 && ey + height > y && ey < y + 2.0 && ez + half > z &&
               ez - half < z + 1.0;
    };
    for (const entity::ServerEntity* other : world.state().entities()) {
        if (!other->removed && overlaps(other->x, other->y, other->z, other->width(), other->height())) {
            return false;
        }
    }
    for (const world::DroppedItem& item : world.state().items()) {
        if (overlaps(item.x, item.y, item.z, 0.25, 0.25)) {
            return false;
        }
    }
    if (context.registry != nullptr) {
        for (const PlayerSession* other : context.registry->snapshot()) {
            if (other->worldName == world.name() && !other->dead && overlaps(other->x, other->y, other->z, 0.6, 1.8)) {
                return false;
            }
        }
    }
    const reg::EntityTypeEntry* entry = reg::entityTypes::of(Identifier::vanilla("end_crystal"));
    if (entry == nullptr || context.registry == nullptr) {
        return false;
    }
    auto crystal =
        std::make_unique<entity::ServerEntity>(context.registry->allocateEntityId(), wire::common::Uuid::random(),
                                               entry->id, entry->protocolId, x + 0.5, static_cast<double>(y), z + 0.5);
    world::endCrystals::setShowBottom(*crystal, false);
    world.state().spawnEntity(std::move(crystal));
    heardBySculk(context, world, "entity_place", x, y, z);
    if (player.gamemode != Gamemode::Creative) {
        storage::StoredItemStack left = held;
        left.count = held.count - 1;
        player.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{left});
        if (context.slotChanged) {
            context.slotChanged(player, heldSlot);
        }
    }
    return true;
}

bool hangFrameFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const Identifier& type,
                  const std::int32_t heldSlot, const storage::StoredItemStack& held, const BlockHit& hit) {
    const reg::EntityTypeEntry* entry = reg::entityTypes::of(type);
    if (entry == nullptr || context.registry == nullptr) {
        return false;
    }
    const std::array<std::int32_t, 3> at = beyond(hit);
    // The two enums are both in vanilla's `Direction` order, which is what makes this a cast.
    const auto facing = static_cast<reg::BlockFace>(static_cast<std::uint8_t>(hit.direction));
    if (!world::itemFrames::canHang(world.state(), at[0], at[1], at[2], facing)) {
        return false;
    }
    const std::array<double, 3> hangs = world::itemFrames::hangingAt(at[0], at[1], at[2], facing);
    auto frame =
        std::make_unique<entity::ServerEntity>(context.registry->allocateEntityId(), wire::common::Uuid::random(), type,
                                               entry->protocolId, hangs[0], hangs[1], hangs[2]);
    frame->sourceNbt = world::itemFrames::freshTag(facing);
    // parity: the spawn packet carries the face a hanging entity is on, which is what the client
    // draws it against.
    frame->objectData = static_cast<std::int32_t>(facing);
    world.state().spawnEntity(std::move(frame));
    // parity: `GameEvent.ENTITY_PLACE` from where the frame ended up.
    world::vibrations::emit(world.state(), "entity_place", hangs[0], hangs[1], hangs[2], world.gameTime());
    if (player.gamemode == Gamemode::Creative) {
        return true;
    }
    storage::StoredItemStack left = held;
    left.count = held.count - 1;
    player.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{left});
    if (context.slotChanged) {
        context.slotChanged(player, heldSlot);
    }
    return true;
}

bool useSpawnEggFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const Identifier& egg, const std::int32_t heldSlot, const storage::StoredItemStack& held,
                    const BlockHit& hit) {
    const std::string path{egg.path()};
    const Identifier type = Identifier::vanilla(path.substr(0, path.size() - std::string_view{"_spawn_egg"}.size()));
    const std::optional<std::int32_t> clicked = world.state().blockState(hit.x, hit.y, hit.z);
    if (reg::entityTypes::of(type) == nullptr || !clicked.has_value()) {
        return false;
    }
    // parity: the mob lands on the face that was clicked, unless the block has no collision at all.
    const std::array<std::int32_t, 3> at =
        reg::blockShapes::isEmptyShape(*clicked) ? std::array<std::int32_t, 3>{hit.x, hit.y, hit.z} : beyond(hit);
    if (!spawnFromItem(context, player, world, type, heldSlot, held, at[0] + 0.5, static_cast<double>(at[1]),
                       at[2] + 0.5)) {
        return false;
    }
    // parity: `SpawnEggItem.spawnsFrom` raises `ENTITY_PLACE` at the square the mob lands on.
    heardBySculk(context, world, "entity_place", at[0], at[1], at[2]);
    return true;
}

} // namespace basalt::server::runtime::player

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Paintings.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cmath>
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

[[nodiscard]] std::array<std::int32_t, 3> beyond(const BlockHit& hit) {
    const wire::protocol::play::BlockPos at = wire::protocol::play::BlockPos{hit.x, hit.y, hit.z}.offset(hit.direction);
    return {at.x, at.y, at.z};
}

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
    const std::string path{item.path()};
    const Identifier type =
        path == "minecart"
            ? item
            : Identifier::vanilla(path.substr(0, path.size() - std::string_view{"_minecart"}.size()) + "_minecart");
    // parity: under `minecart_improvements` a cart is not placed into another one.
    if (context.minecartImprovements && context.minecartImprovements()) {
        const entity::Aabb box = entity::Aabb::of(hit.x + 0.5, hit.y + kMinecartRestHeight + slope, hit.z + 0.5,
                                                  static_cast<double>(0.98F), static_cast<double>(0.7F));
        for (const entity::ServerEntity* other : world.state().entities()) {
            if (!other->removed && entity::minecarts::isMinecart(other->type()) &&
                entity::Aabb::of(other->x, other->y, other->z, other->width(), other->height()).intersects(box)) {
                return false;
            }
        }
    }
    if (!spawnFromItem(context, player, world, type, heldSlot, held, hit.x + 0.5, hit.y + kMinecartRestHeight + slope,
                       hit.z + 0.5)) {
        return false;
    }
    // parity: `MinecartItem` raises `ENTITY_PLACE` at the rail's own square, not at the cart.
    heardBySculk(context, world, "entity_place", hit.x, hit.y, hit.z, {.entity = player.uuid()});
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
                            world.gameTime(), {.entity = player.uuid()});
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
    heardBySculk(context, world, "entity_place", x, y, z, {.entity = player.uuid()});
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
    // parity: `PaintingItem.useOn` picks the biggest art that fits.
    if (type.path() == "painting") {
        random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
        const std::optional<registry::vanillaRegistryData::Painting> art =
            world::paintings::fitting(world.state(), at[0], at[1], at[2], facing, random);
        if (!art.has_value()) {
            return false;
        }
        const std::array<double, 3> middle =
            world::paintings::hangingAt(at[0], at[1], at[2], facing, art->width, art->height);
        auto painting =
            std::make_unique<entity::ServerEntity>(context.registry->allocateEntityId(), wire::common::Uuid::random(),
                                                   type, entry->protocolId, middle[0], middle[1], middle[2]);
        wire::nbt::NbtCompound tag = world::itemFrames::freshTag(facing);
        tag.putString("variant", "minecraft:" + std::string{art->id});
        painting->sourceNbt = std::move(tag);
        painting->objectData = static_cast<std::int32_t>(facing);
        world.state().spawnEntity(std::move(painting));
        world::vibrations::emit(world.state(), "entity_place", middle[0], middle[1], middle[2], world.gameTime());
        if (player.gamemode == Gamemode::Creative) {
            return true;
        }
        storage::StoredItemStack spent = held;
        spent.count = held.count - 1;
        player.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{spent});
        if (context.slotChanged) {
            context.slotChanged(player, heldSlot);
        }
        return true;
    }
    if (!world::itemFrames::canHang(world.state(), at[0], at[1], at[2], facing)) {
        return false;
    }
    const std::array<double, 3> hangs = world::itemFrames::hangingAt(at[0], at[1], at[2], facing);
    auto frame =
        std::make_unique<entity::ServerEntity>(context.registry->allocateEntityId(), wire::common::Uuid::random(), type,
                                               entry->protocolId, hangs[0], hangs[1], hangs[2]);
    frame->sourceNbt = world::itemFrames::freshTag(facing);
    // parity: the spawn packet carries the face a hanging entity is on, which the client draws it against.
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

bool placeCushionFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     const std::string_view colour, const std::int32_t heldSlot, const storage::StoredItemStack& held,
                     const BlockHit& hit) {
    const reg::EntityTypeEntry* entry = reg::entityTypes::of(world::cushions::type());
    const std::optional<std::int32_t> clicked = world.state().blockState(hit.x, hit.y, hit.z);
    if (entry == nullptr || context.registry == nullptr || !clicked.has_value()) {
        return false;
    }
    const world::cushions::Tagged tagged = [&context](std::string_view tag, const Identifier& block) {
        return context.blockTags != nullptr && world::realBlockTags::contains(*context.blockTags, tag, block);
    };
    bool onTop = hit.direction == wire::protocol::play::Direction::Up;
    double clickY = hit.y + static_cast<double>(hit.clickY);
    // parity: `recalculateContextForSpecialCollisionShapes`.
    const Identifier* clickedBlock = reg::blockStateDefinitions::blockOf(*clicked);
    if (clickedBlock != nullptr && tagged("minecraft:cushion_uses_collision_shape", *clickedBlock)) {
        const std::array<double, 3> eye{player.x, player.y + world::fluids::eyeHeight(player.sneaking), player.z};
        const std::array<double, 3> at{hit.x + static_cast<double>(hit.clickX), clickY,
                                       hit.z + static_cast<double>(hit.clickZ)};
        const std::array<double, 3> ray{at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
        const double length = std::sqrt((ray[0] * ray[0]) + (ray[1] * ray[1]) + (ray[2] * ray[2]));
        if (length > 0.0) {
            constexpr double kBeyond = 0.001;
            const std::array<double, 3> to{at[0] + (ray[0] / length * kBeyond), at[1] + (ray[1] / length * kBeyond),
                                           at[2] + (ray[2] / length * kBeyond)};
            if (const std::optional<world::cushions::Clip> inside =
                    world::cushions::clipCollision(*clicked, hit.x, hit.y, hit.z, eye, to);
                inside.has_value()) {
                onTop = inside->up;
                clickY = inside->y;
            }
        }
    }
    if (!onTop) {
        return false;
    }
    // parity: `BlockPlaceContext.getClickedPos`, the clicked block when it would be replaced.
    const bool replaced = reg::blockShapes::isReplaceable(*clicked);
    const std::array<std::int32_t, 3> square = replaced ? std::array<std::int32_t, 3>{hit.x, hit.y, hit.z}
                                                        : std::array<std::int32_t, 3>{hit.x, hit.y + 1, hit.z};
    const double x = square[0] + 0.5;
    const double z = square[2] + 0.5;
    if (!world::cushions::canBePlacedAt(world.state(), x, clickY, z, tagged) ||
        world::cushions::occupied(world.state(), x, clickY, z)) {
        return false;
    }
    auto cushion = std::make_unique<entity::ServerEntity>(
        context.registry->allocateEntityId(), wire::common::Uuid::random(), world::cushions::type(), entry->protocolId,
        x, clickY, z, world::cushions::snappedYaw(player.yaw), 0.0F);
    wire::nbt::NbtCompound tag = world::cushions::freshTag(colour, square);
    if (const wire::nbt::NbtTag* name = held.components.find("minecraft:custom_name"); name != nullptr) {
        tag.set("CustomName", *name);
    }
    cushion->sourceNbt = std::move(tag);
    world.state().spawnEntity(std::move(cushion));
    for (const PlayerSession* listener : playersIn(context, world.name())) {
        playSoundFor(*listener, "minecraft:entity.cushion.place", wire::protocol::play::SoundSource::Blocks, x, clickY,
                     z, 0.75F, 0.8F, context.nextSeed ? context.nextSeed() : 0);
    }
    world::vibrations::emit(world.state(), "entity_place", x, clickY, z, world.gameTime(), {.entity = player.uuid()});
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
    heardBySculk(context, world, "entity_place", at[0], at[1], at[2], {.entity = player.uuid()});
    return true;
}

} // namespace basalt::server::runtime::player

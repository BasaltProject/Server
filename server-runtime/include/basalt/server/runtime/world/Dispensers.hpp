#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>
#include <basalt/server/runtime/world/WitherPatterns.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The two blocks that throw their contents at the world when a signal reaches them. parity:
/// `DispenserBlock.neighborChanged`, which books four ticks and remembers it was triggered, and
/// `DropperBlock.dispenseFrom`. A dispenser with nothing in it plays the click and does nothing.
///
/// The behaviours that make an entity are not here and fall back to being thrown.
namespace dispensers {

/// parity: `DispenserBlock`'s own nine slots.
inline constexpr std::int32_t kSize = 9;

/// What a booked turn is filed under. Both blocks share it, as vanilla's own scheduled tick does.
[[nodiscard]] const wire::common::Identifier& tickType();

/// parity: `DispenserBlock.neighborChanged`, which listens at its own block *and* the one above,
/// and only books a turn on the rising edge.
[[nodiscard]] std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName,
                                                           std::int32_t x, std::int32_t y, std::int32_t z);

/// What a dispenser needs from the rest of the server to do its turn.
///
/// Every field falls back to the registry when it is left unset, so a caller that has nothing
/// special to say passes `{}` and still gets vanilla behaviour.
struct Hooks {
    /// A free entity id for whatever it throws or shoots.
    std::function<std::int32_t()> allocateEntityId;
    /// Whether a state is in `minecraft:copper`, which the carved pumpkin entry needs to know
    /// whether a copper golem would stand up. Unset, only snow and iron ones can.
    golemPatterns::IsCopper isCopper;
    /// How many slots the container in front has, or nothing when it is not one. Defaults to
    /// `containers::sizeOf`.
    std::function<std::optional<std::int32_t>(std::string_view path)> containerSize;
    /// The protocol number a projectile of this type spawns with. Defaults to `entityTypes::of`.
    std::function<std::optional<std::int32_t>(const wire::common::Identifier& type)> protocolTypeOf;
    /// parity: a dropper hands its stack to `HopperBlockEntity.addItem`, so filling the empty input
    /// slot of a furnace goes through `setItem` and sets the cooking time. The recipe book is the
    /// world's, so the caller answers it.
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    /// parity: the bucket entries of `DispenseItemBehavior`, which raise `FLUID_PLACE` and
    /// `FLUID_PICKUP`. A vibration is booked for the turn it arrives, so the turn has to be known.
    std::int64_t gameTime = 0;
    /// Whether a state is in `minecraft:wither_summon_base_blocks`, for the skull entry. Unset, a
    /// dispenser never finishes a wither.
    witherPatterns::IsBase isWitherBase;
    /// A wither skull a dispenser just put down, which may be the one that summons the boss.
    /// parity: `WitherSkullBlock.checkSpawn` from the dispenser behaviour.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)> skullPlaced;
    /// parity: `BlockTags.CONVERTABLE_TO_MUD`, which a dispensed water bottle asks. The tag is the
    /// datapack's, so the caller answers it.
    std::function<bool(const wire::common::Identifier& block)> convertsToMud;
    /// parity: `FireworkRocketItem.createDispenseConfig`'s rocket, shot at an angle from just outside
    /// the face along the way it faces. Unset, a rocket is thrown like any other item.
    std::function<void(double x, double y, double z, double dirX, double dirY, double dirZ,
                       const server::world::loot::ItemStack& stack)>
        launchFirework;
    /// parity: `BeehiveBlock.releaseBeesAndResetHoneyLevel(..., BEE_RELEASED)` after a dispenser
    /// took the honey; `stateId` is the hive as it was.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)> releaseHive;
};

/// What a turn did: whether anything went out, and what the clients should hear about it.
struct Result {
    bool fired = false;
    std::vector<BlockEffect> effects;
    /// The blocks a behaviour changed, which is bone meal growing what is in front of it.
    std::vector<BlockChange> changes;
};

/// The turn itself: one item out of a slot picked at random, into the container in front or into
/// the world. parity: `DispenserBlock.dispenseFrom` and `DropperBlock`'s own.
///
/// The context is the fluid one because a bucket in a dispenser pours and fills like a player's,
/// which needs the game rules and the random behind a flow as well as the world.
[[nodiscard]] Result fire(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                          const Hooks& hooks = {});

} // namespace dispensers

} // namespace basalt::server::runtime::world

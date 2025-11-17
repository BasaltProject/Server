#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

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
    /// How many slots the container in front has, or nothing when it is not one. Defaults to
    /// `containers::sizeOf`.
    std::function<std::optional<std::int32_t>(std::string_view path)> containerSize;
    /// The protocol number a projectile of this type spawns with. Defaults to `entityTypes::of`.
    std::function<std::optional<std::int32_t>(const wire::common::Identifier& type)> protocolTypeOf;
};

/// What a turn did: whether anything went out, and what the clients should hear about it.
struct Result {
    bool fired = false;
    std::vector<BlockEffect> effects;
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

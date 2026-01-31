#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The one entity a comparator reads.
///
/// parity: `ComparatorBlock.getItemFrame` and `ItemFrame.getAnalogOutput`. A frame only counts on
/// the far side of a solid block, which is where `ComparatorBlock.getInputSignal` looks for it, and
/// two frames in the same square cancel each other out.
namespace itemFrames {

[[nodiscard]] bool isFrame(const wire::common::Identifier& type);

/// What clicking one did. parity: `ItemFrame.interact`, which puts an item in an empty frame and
/// turns the one already in a full one.
enum class Touched : std::uint8_t { Nothing, Took, Rotated };

/// The square a frame hangs in, which is where a comparator looks for it.
[[nodiscard]] std::array<std::int32_t, 3> squareOf(const entity::ServerEntity& frame);

/// parity: `ItemFrame.interact`. The stack is what the player is holding, and only one of it goes in.
[[nodiscard]] Touched interact(entity::ServerEntity& frame, const std::optional<server::world::loot::ItemStack>& held);

/// What the frame is showing, for the client and for whoever breaks it.
[[nodiscard]] std::optional<server::world::loot::ItemStack> shownIn(const entity::ServerEntity& frame);

/// parity: `ItemFrame.getRotation`, the eighth of a turn the item is drawn at.
[[nodiscard]] std::int32_t rotationOf(const entity::ServerEntity& frame);

/// What the one frame in this square facing this way reads as, or nothing when there is not exactly
/// one. An empty frame answers zero, which is not the same as answering nothing.
[[nodiscard]] std::optional<std::int32_t> analogOutput(const WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, server::registry::BlockFace facing);

} // namespace itemFrames

} // namespace basalt::server::runtime::world

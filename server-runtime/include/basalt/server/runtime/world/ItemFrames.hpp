#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `ComparatorBlock.getItemFrame`.
namespace itemFrames {

[[nodiscard]] bool isFrame(const wire::common::Identifier& type);

/// parity: `HangingEntity.survives`.
[[nodiscard]] bool canHang(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           server::registry::BlockFace facing);

/// parity: `ItemFrame.survives`.
[[nodiscard]] bool survives(const WorldState& world, const entity::ServerEntity& frame);

/// parity: the tag `addAdditionalSaveData` writes for a frame nobody has touched yet.
[[nodiscard]] wire::nbt::NbtCompound freshTag(server::registry::BlockFace facing);

/// parity: `ItemFrame.createBoundingBox`.
[[nodiscard]] std::array<double, 3> hangingAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                              server::registry::BlockFace facing);

/// parity: `ItemFrame.interact` puts an item in an empty frame and turns a full one.
enum class Touched : std::uint8_t { Nothing, Took, Rotated };

[[nodiscard]] std::array<std::int32_t, 3> squareOf(const entity::ServerEntity& frame);

/// parity: `ItemFrame.createBoundingBox`.
[[nodiscard]] std::array<double, 6> boundingBox(const entity::ServerEntity& frame);

/// parity: `ItemFrame.interact`; only one of the held stack goes in.
[[nodiscard]] Touched interact(entity::ServerEntity& frame, const std::optional<server::world::loot::ItemStack>& held);

[[nodiscard]] std::optional<server::world::loot::ItemStack> shownIn(const entity::ServerEntity& frame);

/// parity: `ItemFrame.getRotation`, the eighth of a turn the item is drawn at.
[[nodiscard]] std::int32_t rotationOf(const entity::ServerEntity& frame);

/// An empty frame answers zero, which is not the same as answering nothing.
[[nodiscard]] std::optional<std::int32_t> analogOutput(const WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, server::registry::BlockFace facing);

} // namespace itemFrames

} // namespace basalt::server::runtime::world

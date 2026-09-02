#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::runtime::world {

/// The shulker box: a chest whose contents go with it when it is picked up.
///
/// parity: `ShulkerBoxBlock.getDrops` puts the block entity's slots into the item's
/// `minecraft:container` component, and `BlockItem.updateCustomBlockEntityTag` writes them back into
/// the block when it is placed. That round trip is the whole point of the block, so it is here rather
/// than in the loot table: the table's `set_contents` is what does it in vanilla.
namespace shulkers {

/// parity: `ShulkerBoxBlockEntity`, which is a chest's twenty-seven.
inline constexpr std::int32_t kSize = 27;

/// parity: `DataComponents.CONTAINER`, which is where the contents ride.
inline constexpr std::string_view kContainer = "minecraft:container";

/// Every shulker box there is: the plain one and one per dye.
[[nodiscard]] std::span<const std::string_view> blocks();

[[nodiscard]] bool isShulkerBox(std::string_view path);

/// The contents of a box, in the form an item component takes, or nothing when it is empty.
///
/// `data` is the block entity's own tag: the caller looks the chunk up, since a box is only ever
/// asked about while its block is being broken or placed.
[[nodiscard]] std::optional<wire::nbt::NbtCompound> contentsOf(const wire::nbt::NbtCompound& data);

/// Puts contents carried by an item back into the box that was just placed.
void restore(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& carried);

} // namespace shulkers

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The decorated pot, which holds one stack and nothing else.
///
/// parity: `DecoratedPotBlock` over `DecoratedPotBlockEntity`. It is a container with one slot and no
/// screen: items go in one at a time by hand, a hopper fills and empties it, and a comparator reads
/// how full that single stack is. Clicking it with an empty hand only makes it wobble.
///
/// Everything here takes the block entity's own tag rather than a world: the caller has already
/// looked the pot up to know it is one.
namespace decoratedPots {

/// parity: `ContainerSingleItem`, which is exactly one slot.
inline constexpr std::int32_t kSize = 1;

/// parity: `DecoratedPotBlock.SHERDS_DYNAMIC_DROP_ID`, the name its loot table asks the block for.
inline constexpr std::string_view kSherds = "minecraft:sherds";

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isPot(std::string_view path);

/// What is in the pot, or nothing when it is empty.
[[nodiscard]] std::optional<server::world::loot::ItemStack> contents(const wire::nbt::NbtCompound& data);

/// parity: `DecoratedPotBlock.useItemOn`: one item goes in, and only if the pot is empty or already
/// holds that same item with room left. Answers the effects, or nothing when it did not fit.
[[nodiscard]] std::optional<std::vector<BlockEffect>> insert(wire::nbt::NbtCompound& data,
                                                             const wire::common::Identifier& item);

/// parity: `DecoratedPotBlock.useWithoutItem`, which is a wobble and a thud and nothing else.
[[nodiscard]] std::vector<BlockEffect> rebuff();

/// The sherds a broken pot leaves: left, back, front and right, and nothing for a bare side.
///
/// parity: 26.3 `DecoratedPotBlock.getDrops`, which hands them to the `sherds` dynamic drop of its
/// loot table. A 26.2 save's list of four reads as those four, brick included.
[[nodiscard]] std::vector<server::world::loot::ItemStack> sherds(const wire::nbt::NbtCompound& data);

/// parity: `DecoratedPotBlockEntity.applyImplicitComponents`: the item's `pot_decorations` become
/// the pot's `sherds`, side by side.
void decorate(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& components);

/// parity: `RandomizableContainer`, which a hopper empties one item at a time like any other.
[[nodiscard]] std::optional<server::world::loot::ItemStack> takeOne(wire::nbt::NbtCompound& data);

} // namespace decoratedPots

} // namespace basalt::server::runtime::world

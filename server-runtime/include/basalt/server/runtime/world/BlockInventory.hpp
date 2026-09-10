#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The slots a block keeps, in the shape vanilla writes them.
///
/// parity: `ContainerHelper.saveAllItems`/`loadAllItems`: a list of compounds, each with the slot it
/// sits in, the item and the count. A furnace and a chest differ in how many slots they have and in
/// what they do with them, not in how they are written down.
namespace blockInventory {

using Slots = std::vector<std::optional<server::world::loot::ItemStack>>;

/// Reads `size` slots out of `data`, leaving the ones it does not mention empty.
[[nodiscard]] Slots read(const wire::nbt::NbtCompound& data, std::int32_t size);

/// Writes them back, skipping the empty ones exactly as vanilla does.
void write(wire::nbt::NbtCompound& data, const Slots& slots);

/// The same pair, for a block that keeps its contents somewhere else. parity: a decorated pot holds
/// one stack under `item`, not a list, so reading it as a list finds nothing and writing it as one
/// leaves a tag vanilla never wrote.
[[nodiscard]] Slots readFor(std::string_view path, const wire::nbt::NbtCompound& data, std::int32_t size);

void writeFor(std::string_view path, wire::nbt::NbtCompound& data, const Slots& slots);

/// Whether this block keeps a single stack under its own key rather than an item list.
[[nodiscard]] bool holdsOneItem(std::string_view path);

} // namespace blockInventory

} // namespace basalt::server::runtime::world

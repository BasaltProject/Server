#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
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

} // namespace blockInventory

} // namespace basalt::server::runtime::world

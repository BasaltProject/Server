#pragma once

#include <basalt/server/runtime/world/BaseSpawnerData.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What a mob wears out of a loot table.
///
/// parity: `EquipmentUser.equip`, which a spawner's `equipment` and a trial spawner both use.
namespace equipmentTables {

/// parity: `EquipmentUser.resolveSlot`'s reading of the stack's `equippable` component: the slot a
/// stack of this item asks for, or nothing for an item held in the hand.
[[nodiscard]] std::optional<std::string_view> equippableSlot(const server::world::loot::ItemStack& stack);

/// Puts what the table rolled on the mob saved in `data`, under `equipment` and `drop_chances`.
/// parity: `EquipmentUser.equip`: first stack per slot wins, an armour slot keeps only one of a stack,
/// and a filled slot takes the table's drop chance when it names one.
void equip(wire::nbt::NbtCompound& data, const EquipmentTable& table,
           const std::vector<server::world::loot::ItemStack>& stacks);

} // namespace equipmentTables

} // namespace basalt::server::runtime::world

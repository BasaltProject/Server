#pragma once

#include <basalt/server/runtime/world/BaseSpawnerData.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `EquipmentUser.equip`, used by a spawner's `equipment` and a trial spawner.
namespace equipmentTables {

/// parity: `EquipmentUser.resolveSlot`.
[[nodiscard]] std::optional<std::string_view> equippableSlot(const server::world::loot::ItemStack& stack);

/// parity: `EquipmentUser.equip`.
void equip(wire::nbt::NbtCompound& data, const EquipmentTable& table,
           const std::vector<server::world::loot::ItemStack>& stacks);

} // namespace equipmentTables

} // namespace basalt::server::runtime::world

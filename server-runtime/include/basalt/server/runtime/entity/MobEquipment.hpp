#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a mob wears and holds, kept in its saved `equipment` and `drop_chances`.
///
/// parity: `Mob.getItemBySlot` and `setItemSlot`, over the saved form this build keeps a mob in.
namespace mobEquipment {

/// parity: `EquipmentSlot`, in the order the equipment packet numbers them.
enum class Slot : std::uint8_t { MainHand, OffHand, Feet, Legs, Chest, Head, Body, Saddle };

[[nodiscard]] std::string_view keyOf(Slot slot);

[[nodiscard]] std::optional<server::world::loot::ItemStack> get(const ServerEntity& mob, Slot slot);

/// Puts a stack in a slot, or empties it. `guaranteed` is `setItemSlotAndDropWhenKilled`.
void set(ServerEntity& mob, Slot slot, const std::optional<server::world::loot::ItemStack>& stack, bool guaranteed);

[[nodiscard]] bool holds(const ServerEntity& mob, std::string_view itemPath);

} // namespace mobEquipment

} // namespace basalt::server::runtime::entity

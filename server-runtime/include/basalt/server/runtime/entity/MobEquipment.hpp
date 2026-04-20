#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobEquipment {

enum class Slot : std::uint8_t { MainHand, OffHand, Feet, Legs, Chest, Head, Body, Saddle };

[[nodiscard]] std::string_view keyOf(Slot slot);

[[nodiscard]] std::optional<server::world::loot::ItemStack> get(const ServerEntity& mob, Slot slot);

void set(ServerEntity& mob, Slot slot, const std::optional<server::world::loot::ItemStack>& stack, bool guaranteed);

[[nodiscard]] bool holds(const ServerEntity& mob, std::string_view itemPath);

} // namespace mobEquipment

} // namespace basalt::server::runtime::entity

#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

namespace blockInventory {

using Slots = std::vector<std::optional<server::world::loot::ItemStack>>;

[[nodiscard]] std::optional<server::world::loot::ItemStack> readStack(const wire::nbt::NbtCompound& stack);

[[nodiscard]] wire::nbt::NbtCompound writeStack(const server::world::loot::ItemStack& stack);

[[nodiscard]] Slots read(const wire::nbt::NbtCompound& data, std::int32_t size);

void write(wire::nbt::NbtCompound& data, const Slots& slots);

/// The same pair, for a block that keeps its contents somewhere else. parity: a decorated pot holds
[[nodiscard]] Slots readFor(std::string_view path, const wire::nbt::NbtCompound& data, std::int32_t size);

void writeFor(std::string_view path, wire::nbt::NbtCompound& data, const Slots& slots);

[[nodiscard]] bool holdsOneItem(std::string_view path);

} // namespace blockInventory

} // namespace basalt::server::runtime::world

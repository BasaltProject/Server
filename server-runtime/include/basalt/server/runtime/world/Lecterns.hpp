#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// parity: `LecternBlock`.
namespace lecterns {

inline constexpr std::int32_t kPulseTicks = 2;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `LecternBlock.tryPlaceBook`'s own check, which takes a book and nothing else.
[[nodiscard]] bool isBook(const wire::common::Identifier& item);

/// parity: `tryPlaceBook` takes a book only on an empty stand.
[[nodiscard]] std::optional<BlockChange> placeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                   const server::world::loot::ItemStack& book);

/// parity: `resolveBook`.
bool resolveBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `resetBookState(false)` plus the book itself.
[[nodiscard]] std::optional<std::pair<BlockChange, server::world::loot::ItemStack>>
takeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `signalPageChange`.
[[nodiscard]] std::optional<BlockChange> turnPage(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t page);

[[nodiscard]] std::optional<BlockChange> endPulse(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LecternBlockEntity.getRedstoneSignal`.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace lecterns

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `AbstractContainerMenu.getRedstoneSignalFromContainer`.
namespace containers {

/// parity: `ChestBlockEntity`.
inline constexpr std::int32_t kChestSize = 27;

[[nodiscard]] std::optional<std::int32_t> sizeOf(std::string_view path);

[[nodiscard]] bool isPlain(std::string_view path);

/// Shelf and crafter read their own way and are still being ported, so they answer by callback.
struct Readers {
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                              std::string_view from)>
        shelf;
    std::function<std::int32_t(const wire::nbt::NbtCompound& data)> crafter;
    std::function<std::int32_t(std::int32_t x, std::int32_t y, std::int32_t z)> cart;
};

/// parity: `Container.setItem`.
[[nodiscard]] bool setItemTellsComparators(std::string_view path, std::int32_t slot, bool sameItem);

/// parity: `Container.getMaxStackSize(stack)`.
[[nodiscard]] std::int32_t maxStackFor(std::string_view path, const server::world::loot::ItemStack& stack);

inline constexpr std::int32_t kSlotLimit = 99;

/// parity: `WorldlyContainer.getSlotsForFace`.
[[nodiscard]] std::vector<std::int32_t> slotsForFace(std::string_view path, std::int32_t size,
                                                     registry::BlockFace from);

/// parity: `canPlaceItemThroughFace` over each container's `canPlaceItem`.
[[nodiscard]] bool acceptsThroughFace(std::string_view path, std::int32_t slot, const wire::common::Identifier& item,
                                      bool slotIsEmpty);

/// parity: `canTakeItemThroughFace`.
[[nodiscard]] bool givesThroughFace(std::string_view path, std::int32_t slot, const wire::common::Identifier& item,
                                    registry::BlockFace from);

/// `from` is the side the comparator sits on, which only a shelf cares about.
[[nodiscard]] std::optional<std::int32_t> analogSignal(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::string_view from = {},
                                                       const Readers& readers = {});

} // namespace containers

} // namespace basalt::server::runtime::world

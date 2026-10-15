#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `ShelfBlock`.
namespace shelves {

inline constexpr std::int32_t kSize = 3;
inline constexpr std::int32_t kRows = 1;
inline constexpr std::int32_t kColumns = 3;

inline constexpr std::int32_t kMaxChain = 3;

using Position = std::array<std::int32_t, 3>;

[[nodiscard]] bool isShelf(std::string_view path);

[[nodiscard]] std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX,
                                                 float clickY, float clickZ);

struct SwapResult {
    std::optional<server::world::loot::ItemStack> removed;
    std::vector<BlockEffect> effects;
};

/// parity: `swapSingleItem`.
[[nodiscard]] std::optional<SwapResult> swap(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t slot,
                                             const std::optional<server::world::loot::ItemStack>& held);

/// parity: `swapHotbar`.
bool swapHotbar(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, blockInventory::Slots& hotbar);

/// parity: `ShelfBlock.neighborChanged`.
void reactToSignal(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                   std::int64_t gameTime, std::vector<BlockChange>& changes);

/// parity: `affectNeighborsAfterRemoval` runs `updateNeighborsAfterPoweringDown` with the state that was there.
void leftBehind(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t wasStateId,
                std::vector<BlockChange>& changes);

/// parity: `ShelfBlock.onPlace`.
void arrived(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool wasConnected,
             std::vector<BlockChange>& changes);

/// parity: `ShelfBlockEntity.setChanged()` raises `BLOCK_ACTIVATE` on every slot write.
void slotsChanged(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime);

/// parity: `getAllBlocksConnectedTo`.
[[nodiscard]] std::vector<Position> chainAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ShelfBlock.getAnalogOutputSignal`, three bits and only read from behind.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound* data, std::string_view facing,
                                            std::string_view from);

} // namespace shelves

} // namespace basalt::server::runtime::world

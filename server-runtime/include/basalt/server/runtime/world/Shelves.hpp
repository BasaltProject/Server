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

/// The shelf: three slots you swap by hand, and a whole hotbar at once when it is powered.
///
/// parity: `ShelfBlock` over `ShelfBlockEntity`. Powering it joins it to the shelves either side into
/// a chain of at most three (`SideChainPartBlock`), and clicking any of them then swaps the player's
/// nine hotbar slots against the chain's nine. A comparator behind it reads which of the three slots
/// are full as three bits, not how full they are.
namespace shelves {

/// parity: `ShelfBlockEntity`, three slots.
inline constexpr std::int32_t kSize = 3;
inline constexpr std::int32_t kRows = 1;
inline constexpr std::int32_t kColumns = 3;

/// parity: `ShelfBlock.getMaxChainLength`, three shelves, which is one hotbar.
inline constexpr std::int32_t kMaxChain = 3;

using Position = std::array<std::int32_t, 3>;

[[nodiscard]] bool isShelf(std::string_view path);

/// Which slot a click lands on, or nothing when it was not on the front of the shelf.
[[nodiscard]] std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX,
                                                 float clickY, float clickZ);

/// What came off the shelf, and what the swap sounded like.
struct SwapResult {
    std::optional<server::world::loot::ItemStack> removed;
    std::vector<BlockEffect> effects;
};

/// parity: `swapSingleItem`: whatever is in the slot comes out and whatever was held goes in, both at
/// once, so an empty hand empties the slot and a full one exchanges.
[[nodiscard]] std::optional<SwapResult> swap(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t slot,
                                             const std::optional<server::world::loot::ItemStack>& held);

/// parity: `swapHotbar`: every shelf in the chain against the hotbar slots that line up with it,
/// counted backwards from the ninth so a chain of one uses the last three.
///
/// `hotbar` is the player's nine slots, swapped in place. Answers false when nothing moved.
bool swapHotbar(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, blockInventory::Slots& hotbar);

/// parity: `ShelfBlock.neighborChanged`: the signal flips `powered`, an unpowered shelf leaves its
/// chain, and a powered one joins whatever is beside it.
void reactToSignal(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                   std::int64_t gameTime, std::vector<BlockChange>& changes);

/// parity: `ShelfBlock.affectNeighborsAfterRemoval`, which runs `updateNeighborsAfterPoweringDown`
/// with the state that **was** there: the shelves either side let go of the square that emptied.
void leftBehind(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t wasStateId,
                std::vector<BlockChange>& changes);

/// parity: `ShelfBlock.onPlace`, which carries **no** "only when the block changed" guard and runs on
/// every write: a shelf that arrives already powered joins the chain beside it, and one that arrives
/// without a signal breaks the chain its square was part of. `isBeingUpdatedByNeighbor` is what keeps
/// the chain from redoing itself while a neighbour is writing it, and that is the `wasConnected`
/// argument: the part the shelf carried before it was written.
void arrived(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool wasConnected,
             std::vector<BlockChange>& changes);

/// parity: `ShelfBlockEntity.setChanged()`, which raises `BLOCK_ACTIVATE` on every write to its
/// slots. No other container makes a noise when a hopper fills it: this one does.
void slotsChanged(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime);

/// parity: `getAllBlocksConnectedTo`, left to right: the shelves that share this one's chain, this
/// one included. A shelf with no power is a chain of nothing.
[[nodiscard]] std::vector<Position> chainAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ShelfBlock.getAnalogOutputSignal`, three bits, one per slot, and only read from behind: a
/// comparator at the side of a shelf gets nothing.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound* data, std::string_view facing,
                                            std::string_view from);

} // namespace shelves

} // namespace basalt::server::runtime::world

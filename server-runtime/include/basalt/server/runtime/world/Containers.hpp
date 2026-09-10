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

/// What a container holds, seen from outside: how full it is, which is all a comparator wants.
///
/// parity: `BlockState.getAnalogOutputSignal` over `AbstractContainerMenu.getRedstoneSignalFromContainer`.
/// Twenty-seven blocks implement it in 26.2; the ones whose contents this build keeps are here, and
/// the rest answer nothing rather than a wrong zero, so a comparator behind them reads the block
/// instead of being told the box is empty.
namespace containers {

/// parity: `ChestBlockEntity`, three rows. A paired chest reads as one of 54; see `doubleChests`.
inline constexpr std::int32_t kChestSize = 27;

/// How many slots each container has, which is the denominator of the fullness.
[[nodiscard]] std::optional<std::int32_t> sizeOf(std::string_view path);

/// The blocks that only hold, with no machinery of their own.
[[nodiscard]] bool isPlain(std::string_view path);

/// The two blocks whose reading is their own business: a shelf answers only from behind, and a
/// crafter counts slots rather than items. Both are still being ported, so they answer by callback.
struct Readers {
    /// Answers the shelf's own reading, or nothing when this is not a shelf.
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                              std::string_view from)>
        shelf;
    /// Answers the crafter's own reading from its tag.
    std::function<std::int32_t(const wire::nbt::NbtCompound& data)> crafter;
    /// Answers what the container minecart standing on a detector rail reads. It takes entities, so
    /// it comes from outside like the other two.
    std::function<std::int32_t(std::int32_t x, std::int32_t y, std::int32_t z)> cart;
};

/// Whether writing one slot from outside (a command, a player's menu) tells the comparators.
///
/// parity: `Container.setItem`. `BaseContainerBlockEntity` ends its in `setChanged`, so a chest or a
/// dispenser says so, but a hopper and a lectern override that away and a furnace only speaks up for
/// its input slot, and only when the item there really changed.
[[nodiscard]] bool setItemTellsComparators(std::string_view path, std::int32_t slot, bool sameItem);

/// How high one item stacks **in this container**, which is not always how high it stacks in a hand.
///
/// parity: `Container.getMaxStackSize(stack)`: three block entities cut it to one, and two of them
/// are within a hopper's reach: a chiseled bookshelf and a jukebox.
[[nodiscard]] std::int32_t maxStackFor(std::string_view path, const wire::common::Identifier& item);

/// The slots a container opens to whoever asks from `from`. parity:
/// `WorldlyContainer.getSlotsForFace`: a furnace and a brewing stand each open different slots per
/// face; everything else opens every slot to every side.
[[nodiscard]] std::vector<std::int32_t> slotsForFace(std::string_view path, std::int32_t size,
                                                     registry::BlockFace from);

/// Whether this item may go into that slot from that side, with `there` being what the slot already
/// holds. parity: `canPlaceItemThroughFace` over each container's own `canPlaceItem`.
[[nodiscard]] bool acceptsThroughFace(std::string_view path, std::int32_t slot, const wire::common::Identifier& item,
                                      bool slotIsEmpty);

/// Whether this item may be taken out of that slot from that side. parity: `canTakeItemThroughFace`,
/// which is what stops a hopper draining a furnace's fuel or a brewing stand's ingredient.
[[nodiscard]] bool givesThroughFace(std::string_view path, std::int32_t slot, const wire::common::Identifier& item,
                                    registry::BlockFace from);

/// The comparator reading of the container at these coordinates, or nothing when the block is not
/// one this build keeps contents for.
///
/// `from` is the side the comparator sits on, which only a shelf cares about.
[[nodiscard]] std::optional<std::int32_t> analogSignal(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::string_view from = {},
                                                       const Readers& readers = {});

} // namespace containers

} // namespace basalt::server::runtime::world

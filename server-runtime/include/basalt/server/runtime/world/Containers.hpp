#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

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
};

/// The comparator reading of the container at these coordinates, or nothing when the block is not
/// one this build keeps contents for.
///
/// `from` is the side the comparator sits on, which only a shelf cares about.
[[nodiscard]] std::optional<std::int32_t> analogSignal(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::string_view from = {},
                                                       const Readers& readers = {});

} // namespace containers

} // namespace basalt::server::runtime::world

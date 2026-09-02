#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// The lectern: a book on a stand, and the page somebody is on.
///
/// parity: `LecternBlock` over `LecternBlockEntity`. Turning a page is a two-tick pulse of redstone
/// (`signalPageChange`), and a comparator behind it reads how far through the book the reader is:
/// `floor(page / (pages - 1) * 14) + 1`, so the first page is one and the last is fifteen.
namespace lecterns {

/// parity: `PAGE_CHANGE_IMPULSE_TICKS`, the two ticks the pulse lasts.
inline constexpr std::int32_t kPulseTicks = 2;

/// parity: a written book carries its own pages; a plain one is a single page.
inline constexpr std::int32_t kDefaultPages = 1;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `LecternBlock.tryPlaceBook`'s own check, which takes a book and nothing else.
[[nodiscard]] bool isBook(const wire::common::Identifier& item);

/// parity: `tryPlaceBook`, which only takes one when the stand is empty.
[[nodiscard]] std::optional<BlockChange> placeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                   const wire::common::Identifier& book,
                                                   std::int32_t pages = kDefaultPages);

/// parity: `resetBookState(false)` plus the book itself, which is what taking it back gives.
[[nodiscard]] std::optional<std::pair<BlockChange, wire::common::Identifier>>
takeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `signalPageChange`: the page moves, the block pulses for two ticks, and the comparator
/// behind it reads the new page. Answers the change, or nothing when there is no book to turn.
[[nodiscard]] std::optional<BlockChange> turnPage(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t page);

/// The other half of the pulse: two ticks later the lectern goes quiet again.
[[nodiscard]] std::optional<BlockChange> endPulse(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LecternBlockEntity.getRedstoneSignal`: how far through the book the reader is, over
/// fourteen, plus one for having a book at all.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace lecterns

} // namespace basalt::server::runtime::world

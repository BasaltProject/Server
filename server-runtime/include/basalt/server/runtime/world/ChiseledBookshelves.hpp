#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// The chiseled bookshelf: six slots, one book each, chosen by where the click landed.
///
/// parity: `ChiseledBookShelfBlock` over `ChiseledBookShelfBlockEntity`. What a comparator reads is
/// not how full it is but *which* slot was touched last, plus one, which is what makes it the only
/// container that can address six different signals.
namespace chiseledBookshelves {

/// parity: `MAX_BOOKS_IN_STORAGE`, laid out two rows of three.
inline constexpr std::int32_t kSize = 6;
inline constexpr std::int32_t kRows = 2;
inline constexpr std::int32_t kColumns = 3;

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isBookshelf(std::string_view path);

/// Which slot a click at these coordinates lands on, or nothing when it was not on the front.
[[nodiscard]] std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX,
                                                 float clickY, float clickZ);

/// What a book being put in or taken out leaves behind: the state, and what it sounded like.
struct Interaction {
    BlockChange change;
    std::optional<wire::common::Identifier> taken;
};

/// parity: `addBook`, which only takes a book into a slot that is empty, and marks that slot as the
/// one a comparator reads.
[[nodiscard]] std::optional<Interaction> putBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t slot, const wire::common::Identifier& book);

/// parity: `removeBook`, the other half, which hands the book back.
[[nodiscard]] std::optional<Interaction> takeBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t slot);

/// parity: `getAnalogOutputSignal`, the last slot touched plus one, so an untouched shelf reads
/// nothing.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

} // namespace chiseledBookshelves

} // namespace basalt::server::runtime::world

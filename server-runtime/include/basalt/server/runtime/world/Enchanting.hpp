#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The enchanting table: what it offers, what it charges and what it puts on.
///
/// parity: `EnchantmentMenu` over `EnchantmentHelper`. Every draw here comes off a `RandomSource`
/// the caller owns and seeds, because the three offers are a function of the player's own seed and
/// have to come back the same until they take one.
namespace enchanting {

/// One enchantment at one level, as the table offers it.
struct Offer {
    wire::common::Identifier enchantment;
    std::int32_t level = 1;
};

/// The tag an enchanting table may offer out of.
inline constexpr std::string_view kInEnchantingTable = "minecraft:in_enchanting_table";

/// parity: `EnchantingTableBlock.BOOKSHELF_OFFSETS`: the ring two out, at the table's height and
/// one above, which is sixteen squares.
[[nodiscard]] std::span<const std::array<std::int32_t, 3>> bookshelfOffsets();

/// Which block is at a square, or nothing where nothing is loaded.
using BlockAt = std::function<std::optional<wire::common::Identifier>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Whether a block is in a tag, by the tag's full name.
using InTag = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

/// parity: `EnchantingTableBlock.isValidBookShelf` counted over the ring: a shelf two out with
/// nothing between it and the table. The world arrives as callbacks so this stays a function of
/// what is where rather than of which door is asking.
[[nodiscard]] std::int32_t bookshelvesAround(const BlockAt& blockAt, const InTag& inTag, std::int32_t x, std::int32_t y,
                                             std::int32_t z);

/// One of the three rows a table offers: what it asks to be held, everything it would put on, and
/// the one of those the client is shown as a hint.
struct Row {
    std::int32_t cost = 0;
    std::vector<Offer> offers;
    std::optional<Offer> clue;
};

/// parity: `EnchantmentMenu.slotsChanged`: the three rows, all of them off the player's own seed,
/// so they come back the same until one is taken. An item nothing can go on gets three empty rows.
[[nodiscard]] std::array<Row, 3> roll(const EnchantmentBooks& books, const storage::StoredItemStack& stack,
                                      std::int32_t seed, std::int32_t bookshelves);

/// parity: `EnchantmentHelper.getEnchantmentCost`, the number drawn for one of the three rows.
[[nodiscard]] std::int32_t costFor(random::RandomSource& random, std::int32_t slot, std::int32_t bookshelves);

/// parity: `EnchantmentHelper.selectEnchantment`: the cost is nudged by the item's enchantability
/// and a roll, and then one is drawn by weight and more are added while the rolls allow it.
[[nodiscard]] std::vector<Offer> select(random::RandomSource& random, const EnchantmentBooks& books,
                                        const storage::StoredItemStack& stack, std::int32_t cost,
                                        std::string_view tag = kInEnchantingTable);

/// parity: `EnchantmentMenu.clickMenuButton`'s own writing-on: a plain book becomes an enchanted
/// one, and what goes on a book is stored rather than worn.
[[nodiscard]] storage::StoredItemStack applied(storage::StoredItemStack stack, std::span<const Offer> offers);

} // namespace enchanting

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <array>
#include <cstdint>
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

/// parity: `EnchantmentHelper.getEnchantmentCost`, the number drawn for one of the three rows.
[[nodiscard]] std::int32_t costFor(random::RandomSource& random, std::int32_t slot, std::int32_t bookshelves);

/// parity: `EnchantmentHelper.selectEnchantment`: the cost is nudged by the item's enchantability
/// and a roll, and then one is drawn by weight and more are added while the rolls allow it.
[[nodiscard]] std::vector<Offer> select(random::RandomSource& random, const EnchantmentBooks& books,
                                        const storage::StoredItemStack& stack, std::int32_t cost,
                                        std::string_view tag = kInEnchantingTable);

} // namespace enchanting

} // namespace basalt::server::runtime::world

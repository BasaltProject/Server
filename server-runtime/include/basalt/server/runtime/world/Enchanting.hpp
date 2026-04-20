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

/// parity: `EnchantmentMenu`.
namespace enchanting {

struct Offer {
    wire::common::Identifier enchantment;
    std::int32_t level = 1;
};

inline constexpr std::string_view kInEnchantingTable = "minecraft:in_enchanting_table";

/// parity: `EnchantingTableBlock.BOOKSHELF_OFFSETS`.
[[nodiscard]] std::span<const std::array<std::int32_t, 3>> bookshelfOffsets();

using BlockAt = std::function<std::optional<wire::common::Identifier>(std::int32_t x, std::int32_t y, std::int32_t z)>;

using InTag = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

/// parity: `isValidBookShelf` counted over the ring.
[[nodiscard]] std::int32_t bookshelvesAround(const BlockAt& blockAt, const InTag& inTag, std::int32_t x, std::int32_t y,
                                             std::int32_t z);

struct Row {
    std::int32_t cost = 0;
    std::vector<Offer> offers;
    std::optional<Offer> clue;
};

/// parity: `EnchantmentMenu.slotsChanged`.
[[nodiscard]] std::array<Row, 3> roll(const EnchantmentBooks& books, const storage::StoredItemStack& stack,
                                      std::int32_t seed, std::int32_t bookshelves);

/// parity: `EnchantmentHelper.getEnchantmentCost`.
[[nodiscard]] std::int32_t costFor(random::RandomSource& random, std::int32_t slot, std::int32_t bookshelves);

/// parity: `EnchantmentHelper.selectEnchantment`.
[[nodiscard]] std::vector<Offer> select(random::RandomSource& random, const EnchantmentBooks& books,
                                        const storage::StoredItemStack& stack, std::int32_t cost,
                                        std::string_view tag = kInEnchantingTable);

/// parity: `EnchantmentMenu.clickMenuButton`.
[[nodiscard]] storage::StoredItemStack applied(storage::StoredItemStack stack, std::span<const Offer> offers);

} // namespace enchanting

} // namespace basalt::server::runtime::world

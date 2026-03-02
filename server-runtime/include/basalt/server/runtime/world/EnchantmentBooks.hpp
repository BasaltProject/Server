#pragma once

#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <set>
#include <string_view>

namespace basalt::server::runtime::world {

/// What the menus read out of the jar's own enchantment files: what each one costs, what it goes
/// on, and which of them will not sit beside each other.
///
/// Every field is a pointer because none of it is this struct's to own, and every answer has a
/// sensible one for an empty jar: nothing goes on anything, and nothing costs anything.
struct EnchantmentBooks {
    const RealEnchantments* enchantments = nullptr;
    const realItemTags::TagMembers* itemTags = nullptr;
    const realEnchantmentTags::TagMembers* enchantmentTags = nullptr;

    [[nodiscard]] const wire::nbt::NbtCompound* definitionOf(const wire::common::Identifier& id) const;

    [[nodiscard]] std::int32_t anvilCost(const wire::common::Identifier& id) const;

    [[nodiscard]] std::int32_t maxLevel(const wire::common::Identifier& id) const;

    /// parity: `Enchantment.getMinCost` and `getMaxCost`, both linear in the level.
    [[nodiscard]] std::int32_t minCost(const wire::common::Identifier& id, std::int32_t level) const;

    [[nodiscard]] std::int32_t maxCost(const wire::common::Identifier& id, std::int32_t level) const;

    /// parity: `Enchantment.definition().weight()`, how often the table picks it over the others.
    [[nodiscard]] std::int32_t weight(const wire::common::Identifier& id) const;

    /// parity: `Enchantment.isPrimaryItem`, which is what a table may put on it rather than what an
    /// anvil may. An enchantment with no `primary_items` of its own falls back to what it goes on.
    [[nodiscard]] bool isPrimaryFor(const wire::common::Identifier& id, const storage::StoredItemStack& stack) const;

    /// Every enchantment in that tag, which is how a table is told what it may offer at all.
    [[nodiscard]] const std::set<wire::common::Identifier>& inTag(std::string_view tag) const;

    /// parity: `Enchantment.canEnchant`, which is the `supported_items` of its own file.
    [[nodiscard]] bool goesOn(const wire::common::Identifier& id, const storage::StoredItemStack& stack) const;

    /// parity: `Enchantment.areCompatible`: each one's exclusive set is asked about the other.
    [[nodiscard]] bool compatible(const wire::common::Identifier& one, const wire::common::Identifier& other) const;

    /// parity: `EnchantmentTags.CURSE`, the only kind a grindstone leaves on.
    [[nodiscard]] bool isCurse(const wire::common::Identifier& id) const;
};

} // namespace basalt::server::runtime::world

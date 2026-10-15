#pragma once

#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <set>
#include <string_view>

namespace basalt::server::runtime::world {

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

    /// parity: `Enchantment.definition().weight()`.
    [[nodiscard]] std::int32_t weight(const wire::common::Identifier& id) const;

    /// parity: `Enchantment.isPrimaryItem`.
    [[nodiscard]] bool isPrimaryFor(const wire::common::Identifier& id, const storage::StoredItemStack& stack) const;

    [[nodiscard]] const std::set<wire::common::Identifier>& inTag(std::string_view tag) const;

    /// parity: `Enchantment.canEnchant`, which is the `supported_items` of its own file.
    [[nodiscard]] bool goesOn(const wire::common::Identifier& id, const storage::StoredItemStack& stack) const;

    /// parity: `Enchantment.areCompatible`.
    [[nodiscard]] bool compatible(const wire::common::Identifier& one, const wire::common::Identifier& other) const;

    /// parity: `EnchantmentTags.CURSE`, the only kind a grindstone leaves on.
    [[nodiscard]] bool isCurse(const wire::common::Identifier& id) const;
};

} // namespace basalt::server::runtime::world

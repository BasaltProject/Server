#pragma once

#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>

namespace basalt::server::runtime::world::anvils {

/// What the enchantment half of an anvil reads, all of it out of the operator's own jar: what each
/// enchantment costs and goes on, and which of them will not sit beside each other.
struct EnchantmentBooks {
    const RealEnchantments* enchantments = nullptr;
    const realItemTags::TagMembers* itemTags = nullptr;
    const realEnchantmentTags::TagMembers* enchantmentTags = nullptr;
};

/// parity: the enchantment loop of `AnvilMenu.createResult`, as the callback `combine` takes. With
/// no jar behind it nothing merges, which is what an anvil does when it knows no enchantments.
[[nodiscard]] MergeEnchantments enchantmentMerge(EnchantmentBooks books);

} // namespace basalt::server::runtime::world::anvils

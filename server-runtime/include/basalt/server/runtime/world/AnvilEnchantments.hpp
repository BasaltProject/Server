#pragma once

#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>

namespace basalt::server::runtime::world::anvils {

/// parity: the enchantment loop of `AnvilMenu.createResult`, as the callback `combine` takes. With
/// no jar behind it nothing merges, which is what an anvil does when it knows no enchantments.
[[nodiscard]] MergeEnchantments enchantmentMerge(EnchantmentBooks books);

} // namespace basalt::server::runtime::world::anvils

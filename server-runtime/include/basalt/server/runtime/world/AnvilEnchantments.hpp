#pragma once

#include <basalt/server/runtime/world/Anvils.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>

namespace basalt::server::runtime::world::anvils {

[[nodiscard]] MergeEnchantments enchantmentMerge(EnchantmentBooks books);

} // namespace basalt::server::runtime::world::anvils

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// The default `minecraft:enchantable` component on vanilla 26.2 items.
///
/// parity: `ToolMaterial` and `ArmorMaterials`' own `enchantmentValue`, plus the handful of items
/// that name one themselves. The material decides it, which is why it is a prefix and a suffix
/// rather than a table of every item.
namespace itemEnchantability {

/// The value, or nothing for an item that cannot be enchanted at all. The name may carry the
/// `minecraft:` prefix or not.
[[nodiscard]] std::optional<std::int32_t> valueOf(std::string_view item);

} // namespace itemEnchantability

} // namespace basalt::server::runtime::world

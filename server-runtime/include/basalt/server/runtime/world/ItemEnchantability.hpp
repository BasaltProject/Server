#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `ToolMaterial`.
namespace itemEnchantability {

[[nodiscard]] std::optional<std::int32_t> valueOf(std::string_view item);

} // namespace itemEnchantability

} // namespace basalt::server::runtime::world

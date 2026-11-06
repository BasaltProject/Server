#pragma once

#include <span>
#include <string_view>

namespace basalt::server::registry {

/// Item ids of a creative tab in display order, empty for tabs with no fixed list (hotbar, search, inventory).
[[nodiscard]] std::span<const std::string_view> creativeTabItems(std::string_view tab);

} // namespace basalt::server::registry

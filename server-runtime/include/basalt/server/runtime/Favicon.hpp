#pragma once

#include <cstdint>
#include <span>

namespace basalt::server::runtime::favicon {

[[nodiscard]] std::span<const std::uint8_t> bytes();

} // namespace basalt::server::runtime::favicon

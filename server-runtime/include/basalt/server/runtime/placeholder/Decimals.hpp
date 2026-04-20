#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::placeholder {

namespace decimals {

/// The most digits worth offering: past this a double is not saying anything true anyway.
constexpr std::int32_t kMaxDigits = 6;

[[nodiscard]] std::optional<std::pair<std::string, std::int32_t>> split(std::string_view params);

[[nodiscard]] std::string format(double value, std::int32_t digits);

} // namespace decimals

} // namespace basalt::server::runtime::placeholder

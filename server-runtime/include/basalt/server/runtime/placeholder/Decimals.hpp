#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::placeholder {

/// Rounding for placeholders that resolve to a number.
///
/// Any numeric placeholder takes a `_<digits>` suffix saying how many decimals to show, so
/// `%basalt_player_x_2%` is `21.71` where `%basalt_player_x%` is `21.70704236703265`. A raw double
/// is the honest answer to "where is the player" and an unreadable one to put on a scoreboard, and
/// neither side should have to win: the file says which it wants.
namespace decimals {

/// The most digits worth offering: past this a double is not saying anything true anyway.
constexpr std::int32_t kMaxDigits = 6;

/// `x_2` to `x` and 2, or nothing when there is no such suffix.
[[nodiscard]] std::optional<std::pair<std::string, std::int32_t>> split(std::string_view params);

[[nodiscard]] std::string format(double value, std::int32_t digits);

} // namespace decimals

} // namespace basalt::server::runtime::placeholder

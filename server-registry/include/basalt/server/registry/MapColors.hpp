#pragma once

#include <cstdint>

namespace basalt::server::registry::mapColors {
// parity: `MapColor.NONE`.
inline constexpr std::int32_t kNone = 0;
inline constexpr std::int32_t kDirt = 10;
inline constexpr std::int32_t kStone = 11;
inline constexpr std::int32_t kWater = 12;
inline constexpr std::int32_t kColorOrange = 15;
inline constexpr std::int32_t kColorBrown = 26;

// parity: `MapColor.Brightness`, whose id is the low two bits of a packed colour.
enum class Brightness : std::uint8_t { Low = 0, Normal = 1, High = 2, Lowest = 3 };

[[nodiscard]] std::int32_t of(std::int32_t stateId);

// parity: `MapColor.getPackedId`.
[[nodiscard]] constexpr std::uint8_t packed(std::int32_t colorId, Brightness brightness) noexcept {
    return static_cast<std::uint8_t>((colorId << 2) | (static_cast<std::int32_t>(brightness) & 3));
}

[[nodiscard]] std::uint32_t argbOf(std::uint8_t packedColor);
} // namespace basalt::server::registry::mapColors

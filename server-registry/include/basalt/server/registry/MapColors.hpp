#pragma once

#include <cstdint>

namespace basalt::server::registry::mapColors {

/// parity: `MapColor.NONE`, `WATER` and the two the Nether's fake ceiling paints with.
inline constexpr std::int32_t kNone = 0;
inline constexpr std::int32_t kDirt = 10;
inline constexpr std::int32_t kStone = 11;
inline constexpr std::int32_t kWater = 12;
inline constexpr std::int32_t kColorOrange = 15;
inline constexpr std::int32_t kColorBrown = 26;

/// parity: `MapColor.Brightness`, whose id is the low two bits of a packed colour.
enum class Brightness : std::uint8_t { Low = 0, Normal = 1, High = 2, Lowest = 3 };

/// The map colour id a block state draws, or `kNone` for an id out of range.
[[nodiscard]] std::int32_t of(std::int32_t stateId);

/// parity: `MapColor.getPackedId`.
[[nodiscard]] constexpr std::uint8_t packed(std::int32_t colorId, Brightness brightness) noexcept {
    return static_cast<std::uint8_t>((colorId << 2) | (static_cast<std::int32_t>(brightness) & 3));
}

/// The opaque ARGB a packed colour is drawn with, or zero for `kNone`. parity:
/// `MapColor.getColorFromPackedId`.
[[nodiscard]] std::uint32_t argbOf(std::uint8_t packedColor);

} // namespace basalt::server::registry::mapColors

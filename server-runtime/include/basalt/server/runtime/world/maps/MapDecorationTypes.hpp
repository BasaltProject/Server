#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::maps {

/// One `map_decoration_type`. parity: `MapDecorationType`, whose registry id is its place in
/// `MapDecorationTypes`.
struct MapDecorationType {
    std::string_view name;
    bool showOnItemFrame = false;
    bool trackCount = false;
};

namespace decorationTypes {

inline constexpr std::int32_t kPlayer = 0;
inline constexpr std::int32_t kFrame = 1;
inline constexpr std::int32_t kPlayerOffMap = 6;
inline constexpr std::int32_t kPlayerOffLimits = 7;
inline constexpr std::int32_t kMansion = 8;

/// The type by registry id, or null.
[[nodiscard]] const MapDecorationType* byId(std::int32_t id);

/// The registry id of `minecraft:<path>` or a full name, or nothing.
[[nodiscard]] std::optional<std::int32_t> idOf(std::string_view name);

/// parity: `MapBanner.getDecoration`, the banner marker of a dye colour by its serialized name.
[[nodiscard]] std::optional<std::int32_t> bannerOf(std::string_view color);

} // namespace decorationTypes

} // namespace basalt::server::runtime::world::maps

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world::maps {

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

[[nodiscard]] const MapDecorationType* byId(std::int32_t id);

[[nodiscard]] std::optional<std::int32_t> idOf(std::string_view name);

/// parity: `MapBanner.getDecoration`.
[[nodiscard]] std::optional<std::int32_t> bannerOf(std::string_view color);

} // namespace decorationTypes

} // namespace basalt::server::runtime::world::maps

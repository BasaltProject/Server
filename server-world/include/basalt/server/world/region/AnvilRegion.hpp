#pragma once

#include <basalt/server/world/region/LinearRegion.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace basalt::server::world::region {

enum class AnvilError : std::uint8_t {
    TooShort,
    OutOfBounds,
    UnsupportedCompression,
    CorruptChunk,
};

[[nodiscard]] std::string_view describe(AnvilError error);

namespace anvilRegion {

[[nodiscard]] std::expected<LinearRegion, AnvilError> read(std::span<const std::uint8_t> bytes);

} // namespace anvilRegion

} // namespace basalt::server::world::region

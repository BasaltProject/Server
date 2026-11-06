#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::datafix {

/// Upgrades unnamed NBT of a data type (`chunk`, `player`, `entity_chunk` or a saved data name) between world versions.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> upgrade(std::string_view type, std::span<const std::uint8_t> nbt,
                                                               std::int32_t fromVersion, std::int32_t toVersion);

[[nodiscard]] bool supports(std::string_view type);

} // namespace basalt::server::plugins::datafix

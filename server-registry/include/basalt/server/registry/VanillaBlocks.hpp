#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace basalt::server::registry {
struct BlockHardnessEntry {
    std::string_view path;
    float destroyTime;
    bool requiresCorrectTool;
    float explosionResistance;
};

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> defaultBlockStatesTable();

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> blockIdsTable();

[[nodiscard]] std::span<const std::string_view> blockStatePropertiesTable();

[[nodiscard]] std::span<const BlockHardnessEntry> blockHardnessTable();
} // namespace basalt::server::registry

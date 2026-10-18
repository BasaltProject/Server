#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::storage {
class ProgressRepository;
struct StoredStats;
} // namespace basalt::server::storage

namespace basalt::server::runtime::world {

/// parity: `AdvancementProgress.OBTAINED_TIME_FORMAT`.
[[nodiscard]] std::optional<std::int64_t> parseObtainedTime(std::string_view text);

/// Answers how many criteria it took; a file that is missing or not JSON takes none.
std::int32_t importVanillaAdvancements(std::string_view json, std::string_view uuid,
                                       storage::ProgressRepository& progress);

/// Reads a vanilla statistics file, flat from before 1.13 or typed, into `stats` with the renames since applied.
std::int32_t importVanillaStats(std::string_view json, storage::StoredStats& stats);

} // namespace basalt::server::runtime::world

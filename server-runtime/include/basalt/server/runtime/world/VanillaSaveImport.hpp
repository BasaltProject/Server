#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::storage {
class PlayerRepository;
class ProgressRepository;
} // namespace basalt::server::storage

namespace basalt::server::runtime::world {

struct ImportReport {
    std::int32_t imported = 0;
    std::int32_t skipped = 0;
    std::int32_t failed = 0;
};

/// Keyed from `usercache.json` because a `.dat` holds no name.
[[nodiscard]] std::expected<ImportReport, std::string>
importVanillaSave(const std::string& savePath, storage::PlayerRepository& repository, const std::string& worldName,
                  std::int64_t nowEpochSeconds, const wire::common::Logger& logger = wire::common::Logger{"Import"},
                  storage::ProgressRepository* progress = nullptr);

} // namespace basalt::server::runtime::world

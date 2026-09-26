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

/// What one import run did, so the command that triggered it can report something truthful.
struct ImportReport {
    std::int32_t imported = 0;
    std::int32_t skipped = 0;
    std::int32_t failed = 0;
};

/// Imports a vanilla save's players into `players.db`, keyed from `usercache.json` because a `.dat`
/// holds no name. One-way and idempotent; with `progress`, advancements come too. The error is why the
/// import could not start (a wrong path, a bad `usercache.json`); an unreadable `.dat` is only counted.
[[nodiscard]] std::expected<ImportReport, std::string>
importVanillaSave(const std::string& savePath, storage::PlayerRepository& repository, const std::string& worldName,
                  std::int64_t nowEpochSeconds, const wire::common::Logger& logger = wire::common::Logger{"Import"},
                  storage::ProgressRepository* progress = nullptr);

} // namespace basalt::server::runtime::world

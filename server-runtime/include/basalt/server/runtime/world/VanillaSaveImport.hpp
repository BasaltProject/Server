#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace basalt::server::storage {
class PlayerRepository;
}

namespace basalt::server::runtime::world {

/// What one import run did, so the command that triggered it can report something truthful.
struct ImportReport {
    std::int32_t imported = 0;
    std::int32_t skipped = 0;
    std::int32_t failed = 0;
};

/// Imports the players of an existing vanilla save into `players.db`, starting from
/// `usercache.json` rather than a listing of `playerdata/`: a `.dat` file holds the data but not the
/// name, so walking the directory would import everyone nameless.
///
/// One-way and idempotent: a second run overwrites the same rows and never writes back into the save
/// it read. The error is why the import could not start at all, a wrong path or a `usercache.json`
/// that is not one; one unreadable `.dat` is counted, not fatal.
[[nodiscard]] std::expected<ImportReport, std::string>
importVanillaSave(const std::string& savePath, storage::PlayerRepository& repository, const std::string& worldName,
                  std::int64_t nowEpochSeconds, const wire::common::Logger& logger = wire::common::Logger{"Import"});

} // namespace basalt::server::runtime::world

#pragma once

#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/server/storage/PluginDataRepository.hpp>
#include <basalt/server/storage/ProgressRepository.hpp>
#include <basalt/server/storage/Sqlite.hpp>

#include <expected>
#include <memory>
#include <string>

namespace basalt::server::storage {

/// The database was written by a build with a different schema, and there are no migrations.
struct PlayerDatabaseSchemaError {
    std::string message;
};

/// The one player database, opened once for the whole server. One file rather than one per player:
/// per-file cannot answer "who has not logged in for a month" without opening every file.
///
/// SQLite is compiled into the binary, so opening a database depends on nothing the host has
/// installed.
class PlayerStore {
public:
    PlayerStore(const PlayerStore&) = delete;
    PlayerStore& operator=(const PlayerStore&) = delete;
    PlayerStore(PlayerStore&&) noexcept = default;
    PlayerStore& operator=(PlayerStore&&) noexcept = default;
    ~PlayerStore();

    /// Opens, creating the file and its directory if they are not there.
    [[nodiscard]] static std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError>
    open(const std::string& path);

    [[nodiscard]] PlayerRepository& players() noexcept { return players_; }

    /// Bans and the whitelist, which the login path consults before anything else.
    [[nodiscard]] AccessRepository& access() noexcept { return access_; }

    /// Advancement criteria and recipe books, loaded at join and written as they change.
    [[nodiscard]] ProgressRepository& progress() noexcept { return progress_; }

    /// What plugins keep about players, each in its own namespace and unable to see another's.
    [[nodiscard]] PluginDataRepository& pluginData() noexcept { return pluginData_; }

    [[nodiscard]] sql::Database& database() noexcept { return *database_; }

    /// False once `close` has run. Callers that may still be winding down on their own threads check
    /// this rather than finding out through a use-after-close.
    [[nodiscard]] bool isOpen() const noexcept { return open_; }

    /// Folds the write-ahead log back into the database file, then closes.
    ///
    /// Not optional even though SQLite checkpoints when the last connection closes: a clean shutdown
    /// left every row in the `-wal` beside a 4 KB `players.db`. Nothing is lost, but backing the
    /// file up alone would copy an empty database.
    void close();

private:
    explicit PlayerStore(std::unique_ptr<sql::Database> database);

    std::unique_ptr<sql::Database> database_;
    PlayerRepository players_;
    AccessRepository access_;
    ProgressRepository progress_;
    PluginDataRepository pluginData_;
    bool open_ = true;
};

} // namespace basalt::server::storage

#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/storage/Schema.hpp>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

namespace basalt::server::storage {
namespace {
[[nodiscard]] std::map<std::string, std::vector<std::string>, std::less<>> referenceColumns() {
    std::map<std::string, std::vector<std::string>, std::less<>> expected;
    std::optional<wire::sql::Database> reference = wire::sql::Database::open(":memory:");
    if (!reference.has_value()) {
        return expected;
    }
    reference->exec(schema::ddl());
    for (const std::string& table : reference->tables()) {
        expected.emplace(table, reference->columns(table));
    }
    return expected;
}

[[nodiscard]] std::optional<PlayerDatabaseSchemaError> verifySchema(wire::sql::Database& database,
                                                                    const std::string& path) {
    for (const auto& [table, columns] : referenceColumns()) {
        const std::vector<std::string> actual = database.columns(table);
        std::vector<std::string> missing;
        for (const std::string& column : columns) {
            if (std::find(actual.begin(), actual.end(), column) == actual.end()) {
                missing.push_back(column);
            }
        }
        if (missing.empty()) {
            continue;
        }
        std::string names;
        for (const std::string& column : missing) {
            if (!names.empty()) {
                names += ", ";
            }
            names += column;
        }
        return PlayerDatabaseSchemaError{path + " was written by an older build: its '" + table + "' table has no " +
                                         names + " column. Basalt has no database migrations yet, so delete " + path +
                                         " (and its -wal and -shm files) and start again. Players will be "
                                         "recreated on their next login."};
    }
    return std::nullopt;
}
} // namespace

PlayerStore::PlayerStore(std::unique_ptr<wire::sql::Database> database)
    : database_(std::move(database)), players_(*database_), access_(*database_), progress_(*database_),
      pluginData_(*database_) {}

PlayerStore::~PlayerStore() { close(); }

std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> PlayerStore::open(const std::string& path) {
    const std::filesystem::path parent = std::filesystem::path{path}.parent_path();
    if (!parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }

    std::optional<wire::sql::Database> opened = wire::sql::Database::open(path);
    if (!opened.has_value()) {
        return std::unexpected(PlayerDatabaseSchemaError{path + " could not be opened"});
    }
    auto database = std::make_unique<wire::sql::Database>(std::move(*opened));
    if (database->tables().empty() && !database->exec(schema::ddl())) {
        return std::unexpected(PlayerDatabaseSchemaError{path + " could not be created: " + database->error()});
    }
    if (std::optional<PlayerDatabaseSchemaError> mismatch = verifySchema(*database, path); mismatch.has_value()) {
        return std::unexpected(*mismatch);
    }
    return std::unique_ptr<PlayerStore>{new PlayerStore{std::move(database)}};
}

std::expected<LegacyDatabaseMove, PlayerDatabaseSchemaError> PlayerStore::moveLegacy(const std::string& from,
                                                                                     const std::string& to) {
    std::error_code error;
    const auto present = [&error](const std::string& path) -> std::expected<bool, PlayerDatabaseSchemaError> {
        const bool found = std::filesystem::exists(path, error);
        if (error) {
            return std::unexpected(PlayerDatabaseSchemaError{path + " could not be inspected: " + error.message()});
        }
        return found;
    };
    const std::expected<bool, PlayerDatabaseSchemaError> legacy = present(from);
    if (!legacy.has_value() || !*legacy) {
        return legacy.transform([](bool) { return LegacyDatabaseMove::NothingToMove; });
    }
    const std::expected<bool, PlayerDatabaseSchemaError> target = present(to);
    if (!target.has_value() || *target) {
        return target.transform([](bool) { return LegacyDatabaseMove::BothPresent; });
    }
    const std::filesystem::path parent = std::filesystem::path{to}.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return std::unexpected(
                PlayerDatabaseSchemaError{parent.string() + " could not be created: " + error.message()});
        }
    }
    // The main file goes last, so an interrupted move is finished by the next start instead of losing the WAL.
    for (const std::string_view suffix : {"-wal", "-shm", ""}) {
        const std::string source = from + std::string{suffix};
        const std::string destination = to + std::string{suffix};
        const std::expected<bool, PlayerDatabaseSchemaError> found = present(source);
        if (!found.has_value()) {
            return std::unexpected(found.error());
        }
        if (!*found) {
            continue;
        }
        std::filesystem::rename(source, destination, error);
        if (error) {
            return std::unexpected(
                PlayerDatabaseSchemaError{source + " could not be moved to " + destination + ": " + error.message()});
        }
    }
    return LegacyDatabaseMove::Moved;
}

void PlayerStore::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    if (database_ != nullptr) {
        database_->exec("PRAGMA wal_checkpoint(TRUNCATE)");
        database_->close();
    }
}
} // namespace basalt::server::storage

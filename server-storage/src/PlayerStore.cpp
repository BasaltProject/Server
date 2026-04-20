#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/storage/Schema.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace basalt::server::storage {
namespace {
[[nodiscard]] std::map<std::string, std::vector<std::string>, std::less<>> referenceColumns() {
    std::map<std::string, std::vector<std::string>, std::less<>> expected;
    std::optional<sql::Database> reference = sql::Database::open(":memory:");
    if (!reference.has_value()) {
        return expected;
    }
    reference->exec(schema::ddl());
    for (const std::string& table : reference->tables()) {
        expected.emplace(table, reference->columns(table));
    }
    return expected;
}

[[nodiscard]] std::optional<PlayerDatabaseSchemaError> verifySchema(sql::Database& database, const std::string& path) {
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

PlayerStore::PlayerStore(std::unique_ptr<sql::Database> database)
    : database_(std::move(database)), players_(*database_), access_(*database_), progress_(*database_),
      pluginData_(*database_) {}

PlayerStore::~PlayerStore() { close(); }

std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> PlayerStore::open(const std::string& path) {
    const std::filesystem::path parent = std::filesystem::path{path}.parent_path();
    if (!parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }

    std::optional<sql::Database> opened = sql::Database::open(path);
    if (!opened.has_value()) {
        return std::unexpected(PlayerDatabaseSchemaError{path + " could not be opened"});
    }
    auto database = std::make_unique<sql::Database>(std::move(*opened));
    if (database->tables().empty() && !database->exec(schema::ddl())) {
        return std::unexpected(PlayerDatabaseSchemaError{path + " could not be created: " + database->error()});
    }
    if (std::optional<PlayerDatabaseSchemaError> mismatch = verifySchema(*database, path); mismatch.has_value()) {
        return std::unexpected(*mismatch);
    }
    return std::unique_ptr<PlayerStore>{new PlayerStore{std::move(database)}};
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

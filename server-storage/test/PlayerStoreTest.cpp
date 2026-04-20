#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/storage/PlayerStore.hpp>

#include <doctest.h>

#include <filesystem>
#include <string>

using namespace basalt::server::storage;

namespace {
void removeDatabase(const std::string& path) {
    std::error_code ignored;
    for (const std::string_view suffix : {"", "-wal", "-shm"}) {
        std::filesystem::remove(path + std::string{suffix}, ignored);
    }
}
} // namespace

TEST_CASE("a database missing a column this build needs is refused, not used") {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    const std::string path = (directory / "old_schema.db").string();
    removeDatabase(path);

    {
        const std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> current = PlayerStore::open(path);
        REQUIRE(current.has_value());
        (*current)->close();
        std::optional<sql::Database> old = sql::Database::open(path);
        REQUIRE(old.has_value());
        REQUIRE(old->exec("DROP INDEX player_last_seen"));
        REQUIRE(old->exec("ALTER TABLE player DROP COLUMN last_seen"));
    }

    const std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> opened = PlayerStore::open(path);
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().message.find("its 'player' table has no last_seen column") != std::string::npos);
    CHECK(opened.error().message.find("Basalt has no database migrations yet") != std::string::npos);

    removeDatabase(path);
}

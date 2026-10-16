#include <basalt/server/storage/PluginDataRepository.hpp>

#include <cstdint>

namespace basalt::server::storage {
std::optional<std::string> PluginDataRepository::get(std::string_view plugin, std::string_view uuid,
                                                     std::string_view key) {
    wire::sql::Statement statement =
        database_->prepare("SELECT value FROM plugin_data WHERE plugin = ? AND uuid = ? AND key = ?");
    statement.bind(1, plugin);
    statement.bind(2, uuid);
    statement.bind(3, key);
    if (!statement.step()) {
        return std::nullopt;
    }
    return statement.text(0);
}

std::map<std::string, std::string, std::less<>> PluginDataRepository::all(std::string_view plugin,
                                                                          std::string_view uuid) {
    std::map<std::string, std::string, std::less<>> values;
    wire::sql::Statement statement =
        database_->prepare("SELECT key, value FROM plugin_data WHERE plugin = ? AND uuid = ?");
    statement.bind(1, plugin);
    statement.bind(2, uuid);
    while (statement.step()) {
        values.emplace(statement.text(0), statement.text(1));
    }
    return values;
}

void PluginDataRepository::set(std::string_view plugin, std::string_view uuid, std::string_view key,
                               const std::optional<std::string>& value) {
    if (!value.has_value()) {
        wire::sql::Statement statement =
            database_->prepare("DELETE FROM plugin_data WHERE plugin = ? AND uuid = ? AND key = ?");
        statement.bind(1, plugin);
        statement.bind(2, uuid);
        statement.bind(3, key);
        statement.execute();
        return;
    }
    wire::sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO plugin_data(plugin, uuid, key, value) VALUES (?, ?, ?, ?)");
    statement.bind(1, plugin);
    statement.bind(2, uuid);
    statement.bind(3, key);
    statement.bind(4, *value);
    statement.execute();
}

std::vector<std::pair<std::string, std::string>> PluginDataRepository::top(std::string_view plugin,
                                                                           std::string_view key, std::int32_t limit) {
    std::vector<std::pair<std::string, std::string>> found;
    wire::sql::Statement statement = database_->prepare(
        "SELECT uuid, value FROM plugin_data WHERE plugin = ? AND key = ? ORDER BY CAST(value AS REAL) DESC LIMIT ?");
    statement.bind(1, plugin);
    statement.bind(2, key);
    statement.bind(3, static_cast<std::int64_t>(limit));
    while (statement.step()) {
        found.emplace_back(statement.text(0), statement.text(1));
    }
    return found;
}

std::map<std::string, std::int64_t, std::less<>> PluginDataRepository::namespaces() {
    std::map<std::string, std::int64_t, std::less<>> counts;
    wire::sql::Statement statement =
        database_->prepare("SELECT plugin, COUNT(*) AS rows FROM plugin_data GROUP BY plugin");
    while (statement.step()) {
        counts.emplace(statement.text(0), statement.integer(1));
    }
    return counts;
}

void PluginDataRepository::purge(std::string_view plugin) {
    wire::sql::Statement statement = database_->prepare("DELETE FROM plugin_data WHERE plugin = ?");
    statement.bind(1, plugin);
    statement.execute();
}
} // namespace basalt::server::storage

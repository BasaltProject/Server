#pragma once

#include <basalt/server/storage/Sqlite.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::storage {
class PluginDataRepository {
public:
    explicit PluginDataRepository(sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<std::string> get(std::string_view plugin, std::string_view uuid, std::string_view key);

    [[nodiscard]] std::map<std::string, std::string, std::less<>> all(std::string_view plugin, std::string_view uuid);

    void set(std::string_view plugin, std::string_view uuid, std::string_view key,
             const std::optional<std::string>& value);

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> top(std::string_view plugin, std::string_view key,
                                                                       std::int32_t limit);

    [[nodiscard]] std::map<std::string, std::int64_t, std::less<>> namespaces();

    void purge(std::string_view plugin);

private:
    sql::Database* database_;
};
} // namespace basalt::server::storage

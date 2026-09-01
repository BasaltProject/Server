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

/// What plugins keep about players, one namespace each.
///
/// Every call takes the plugin's id and every query filters on it, which is the whole of the
/// isolation: there is no way to ask this for somebody else's rows, because there is no call that
/// does not name your own. The values are text because a plugin's own data has no schema the server
/// could enforce; what it does enforce is whose data it is.
class PluginDataRepository {
public:
    explicit PluginDataRepository(sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<std::string> get(std::string_view plugin, std::string_view uuid, std::string_view key);

    [[nodiscard]] std::map<std::string, std::string, std::less<>> all(std::string_view plugin, std::string_view uuid);

    /// An absent value deletes the row: a plugin clearing a key should not leave an empty one.
    void set(std::string_view plugin, std::string_view uuid, std::string_view key,
             const std::optional<std::string>& value);

    /// The query a file cannot answer: the highest `key` across every player this plugin knows.
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> top(std::string_view plugin, std::string_view key,
                                                                       std::int32_t limit);

    /// Which plugins have rows here, and how many, for the orphan check at startup.
    [[nodiscard]] std::map<std::string, std::int64_t, std::less<>> namespaces();

    /// What `/plugins purge` removes. Nothing else deletes a plugin's data, ever.
    void purge(std::string_view plugin);

private:
    sql::Database* database_;
};

} // namespace basalt::server::storage

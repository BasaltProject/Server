#pragma once

#include <basalt/server/storage/Sqlite.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::storage {
struct PlayerSummary {
    std::string uuid;
    std::string name;
    std::int64_t value = 0;
};

class PlayerRepository {
public:
    explicit PlayerRepository(sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<StoredPlayer> load(std::string_view uuid);

    [[nodiscard]] std::optional<StoredPlayer> loadByName(std::string_view name);

    [[nodiscard]] bool exists(std::string_view uuid);

    void save(const StoredPlayer& player);

    void saveAll(const std::vector<StoredPlayer>& players);

    bool setGroup(std::string_view uuid, const std::optional<std::string>& group);

    void setLastIp(std::string_view uuid, const std::optional<std::string>& address);

    [[nodiscard]] std::optional<std::string> findNameByAddress(std::string_view address);

    [[nodiscard]] std::vector<PlayerSummary> findInGroup(std::string_view group);

    void remove(std::string_view uuid);

    [[nodiscard]] std::int64_t count();

    [[nodiscard]] std::map<StoredGamemode, std::int64_t> countByGamemode();

    [[nodiscard]] std::vector<PlayerSummary> findLastSeenBefore(std::int64_t epochSeconds);

    [[nodiscard]] std::vector<PlayerSummary> findInWorld(std::string_view world);

    [[nodiscard]] std::vector<PlayerSummary> findWithPlayTimeAtLeast(std::int64_t ticks);

private:
    [[nodiscard]] std::optional<StoredPlayer> loadOne(std::string_view sql, std::string_view key);

    sql::Database* database_;
};
} // namespace basalt::server::storage

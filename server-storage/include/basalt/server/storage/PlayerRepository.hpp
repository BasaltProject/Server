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

/// A player as the admin queries report them: whichever number the query was about.
struct PlayerSummary {
    std::string uuid;
    std::string name;
    std::int64_t value = 0;
};

/// Reading and writing players, and the handful of questions an operator asks about them.
///
/// Every write is one statement against one row, so SQLite's implicit transaction means a player is
/// saved or not, never a new position with the old inventory. That is what this layer is for, and
/// what `playerdata/<uuid>.dat` cannot offer, a truncated file being a lost player.
///
/// The admin queries select columns, never blobs: counting by game mode deserializes no inventory.
class PlayerRepository {
public:
    explicit PlayerRepository(sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<StoredPlayer> load(std::string_view uuid);

    [[nodiscard]] std::optional<StoredPlayer> loadByName(std::string_view name);

    [[nodiscard]] bool exists(std::string_view uuid);

    void save(const StoredPlayer& player);

    /// Saves many players as one transaction, which is how the periodic flush writes.
    ///
    /// Not a micro-optimisation: without it SQLite commits, and in WAL appends and eventually
    /// fsyncs, once per player, so a hundred online players cost a hundred commits every save
    /// interval. It is also the crash-safety story for a shutdown save, since either the whole batch
    /// lands or none of it does.
    void saveAll(const std::vector<StoredPlayer>& players);

    /// Moves a player between groups without touching the rest of their row. False when there is no
    /// such player: this server can only administer people it has seen at least once, having no
    /// profile cache to resolve a name it has never met.
    bool setGroup(std::string_view uuid, const std::optional<std::string>& group);

    /// Remembers the address a player logged in from, for the server list greeting and nothing else.
    /// Separate from `save` so turning the greeting off simply stops calling this, rather than
    /// writing nulls over addresses already recorded.
    void setLastIp(std::string_view uuid, const std::optional<std::string>& address);

    /// The player last seen at `address`, or nothing if this server has never seen anyone there.
    [[nodiscard]] std::optional<std::string> findNameByAddress(std::string_view address);

    [[nodiscard]] std::vector<PlayerSummary> findInGroup(std::string_view group);

    void remove(std::string_view uuid);

    // Admin queries: none of these touch a blob.

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

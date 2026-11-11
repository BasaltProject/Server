#pragma once

#include <basalt/wire/sql/Sqlite.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::storage {
struct StoredSpawnPoint {
    std::string uuid;
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
};

enum class BanKind : std::uint8_t { Player, Ip };

[[nodiscard]] std::string_view storedNameOf(BanKind kind);

struct BanRecord {
    std::string subject;
    BanKind kind = BanKind::Player;
    std::optional<std::string> name;
    std::string reason;
    std::string source;
    std::int64_t createdAtEpochSeconds = 0;
    /// Absent means it never expires, which is every ban vanilla's own commands create.
    std::optional<std::int64_t> expiresAtEpochSeconds;
};

struct WhitelistEntry {
    std::string uuid;
    std::string name;
    std::int64_t addedAtEpochSeconds = 0;
};

struct AccessChange {
    enum class Type : std::uint8_t { WhitelistAdded, WhitelistRemoved, Banned, Pardoned };

    Type type = Type::Banned;
    BanKind kind = BanKind::Player;
    std::string subject;
    std::string name;
    std::optional<BanRecord> ban;
};

class AccessRepository {
public:
    explicit AccessRepository(wire::sql::Database& database) : database_(&database) {}

    [[nodiscard]] std::optional<StoredSpawnPoint> spawnPointOf(std::string_view uuid);

    void setSpawnPoint(const StoredSpawnPoint& point);

    void clearSpawnPoint(std::string_view uuid);

    [[nodiscard]] std::optional<BanRecord> banOf(std::string_view subject, BanKind kind);

    [[nodiscard]] std::vector<BanRecord> bans(BanKind kind);

    void ban(const BanRecord& record);

    bool pardon(std::string_view subject, BanKind kind);

    [[nodiscard]] std::vector<WhitelistEntry> whitelist();

    [[nodiscard]] bool isWhitelisted(std::string_view uuid);

    [[nodiscard]] std::optional<WhitelistEntry> whitelistedByName(std::string_view name);

    void addToWhitelist(const WhitelistEntry& entry);

    bool removeFromWhitelist(std::string_view uuid);

    /// Told about every whitelist and ban change, from whichever thread made it.
    void observe(std::function<void(const AccessChange&)> observer) { observer_ = std::move(observer); }

private:
    wire::sql::Database* database_;
    std::function<void(const AccessChange&)> observer_;
};
} // namespace basalt::server::storage

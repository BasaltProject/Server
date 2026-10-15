#include <basalt/server/storage/AccessRepository.hpp>

namespace basalt::server::storage {
namespace {
constexpr std::string_view kBanColumns = "subject, kind, name, reason, source, created_at, expires_at";
constexpr std::string_view kWhitelistColumns = "uuid, name, added_at";

[[nodiscard]] BanRecord readBan(const sql::Statement& row, BanKind kind) {
    return BanRecord{.subject = row.text(0),
                     .kind = kind,
                     .name = row.textOrNull(2),
                     .reason = row.text(3),
                     .source = row.text(4),
                     .createdAtEpochSeconds = row.integer(5),
                     .expiresAtEpochSeconds = row.integerOrNull(6)};
}

[[nodiscard]] WhitelistEntry readWhitelist(const sql::Statement& row) {
    return WhitelistEntry{.uuid = row.text(0), .name = row.text(1), .addedAtEpochSeconds = row.integer(2)};
}
} // namespace

std::string_view storedNameOf(BanKind kind) { return kind == BanKind::Player ? "player" : "ip"; }

std::optional<StoredSpawnPoint> AccessRepository::spawnPointOf(std::string_view uuid) {
    sql::Statement statement = database_->prepare("SELECT uuid, world, x, y, z, yaw FROM spawn_point WHERE uuid = ?");
    statement.bind(1, uuid);
    if (!statement.step()) {
        return std::nullopt;
    }
    return StoredSpawnPoint{.uuid = statement.text(0),
                            .world = statement.text(1),
                            .x = statement.real(2),
                            .y = statement.real(3),
                            .z = statement.real(4),
                            .yaw = static_cast<float>(statement.real(5))};
}

void AccessRepository::setSpawnPoint(const StoredSpawnPoint& point) {
    sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO spawn_point(uuid, world, x, y, z, yaw) VALUES (?, ?, ?, ?, ?, ?)");
    statement.bind(1, point.uuid);
    statement.bind(2, point.world);
    statement.bind(3, point.x);
    statement.bind(4, point.y);
    statement.bind(5, point.z);
    statement.bind(6, static_cast<double>(point.yaw));
    statement.execute();
}

void AccessRepository::clearSpawnPoint(std::string_view uuid) {
    sql::Statement statement = database_->prepare("DELETE FROM spawn_point WHERE uuid = ?");
    statement.bind(1, uuid);
    statement.execute();
}

std::optional<BanRecord> AccessRepository::banOf(std::string_view subject, BanKind kind) {
    sql::Statement statement =
        database_->prepare("SELECT " + std::string{kBanColumns} + " FROM ban WHERE subject = ? AND kind = ?");
    statement.bind(1, subject);
    statement.bind(2, storedNameOf(kind));
    if (!statement.step()) {
        return std::nullopt;
    }
    return readBan(statement, kind);
}

std::vector<BanRecord> AccessRepository::bans(BanKind kind) {
    std::vector<BanRecord> found;
    sql::Statement statement =
        database_->prepare("SELECT " + std::string{kBanColumns} + " FROM ban WHERE kind = ? ORDER BY created_at");
    statement.bind(1, storedNameOf(kind));
    while (statement.step()) {
        found.push_back(readBan(statement, kind));
    }
    return found;
}

void AccessRepository::ban(const BanRecord& record) {
    sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO ban(subject, kind, name, reason, source, created_at, expires_at) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?)");
    statement.bind(1, record.subject);
    statement.bind(2, storedNameOf(record.kind));
    statement.bind(3, record.name);
    statement.bind(4, record.reason);
    statement.bind(5, record.source);
    statement.bind(6, record.createdAtEpochSeconds);
    statement.bind(7, record.expiresAtEpochSeconds);
    statement.execute();
}

bool AccessRepository::pardon(std::string_view subject, BanKind kind) {
    if (!banOf(subject, kind).has_value()) {
        return false;
    }
    sql::Statement statement = database_->prepare("DELETE FROM ban WHERE subject = ? AND kind = ?");
    statement.bind(1, subject);
    statement.bind(2, storedNameOf(kind));
    return statement.execute();
}

std::vector<WhitelistEntry> AccessRepository::whitelist() {
    std::vector<WhitelistEntry> found;
    sql::Statement statement =
        database_->prepare("SELECT " + std::string{kWhitelistColumns} + " FROM whitelist ORDER BY name");
    while (statement.step()) {
        found.push_back(readWhitelist(statement));
    }
    return found;
}

bool AccessRepository::isWhitelisted(std::string_view uuid) {
    sql::Statement statement = database_->prepare("SELECT 1 FROM whitelist WHERE uuid = ?");
    statement.bind(1, uuid);
    return statement.step();
}

std::optional<WhitelistEntry> AccessRepository::whitelistedByName(std::string_view name) {
    sql::Statement statement = database_->prepare("SELECT " + std::string{kWhitelistColumns} +
                                                  " FROM whitelist WHERE name = ? COLLATE NOCASE LIMIT 1");
    statement.bind(1, name);
    if (!statement.step()) {
        return std::nullopt;
    }
    return readWhitelist(statement);
}

void AccessRepository::addToWhitelist(const WhitelistEntry& entry) {
    sql::Statement statement =
        database_->prepare("INSERT OR REPLACE INTO whitelist(uuid, name, added_at) VALUES (?, ?, ?)");
    statement.bind(1, entry.uuid);
    statement.bind(2, entry.name);
    statement.bind(3, entry.addedAtEpochSeconds);
    statement.execute();
}

bool AccessRepository::removeFromWhitelist(std::string_view uuid) {
    if (!isWhitelisted(uuid)) {
        return false;
    }
    sql::Statement statement = database_->prepare("DELETE FROM whitelist WHERE uuid = ?");
    statement.bind(1, uuid);
    return statement.execute();
}
} // namespace basalt::server::storage

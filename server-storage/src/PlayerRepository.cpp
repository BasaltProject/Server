#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/server/storage/StorageNbt.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::storage {
namespace {

/// The column order the whole-row queries select in, shared so the reader below can be one function.
constexpr std::string_view kColumns =
    "uuid, name, last_seen, first_seen, play_time_ticks, world, x, y, z, yaw, pitch, health, food, "
    "saturation, exhaustion, xp_level, xp_progress, xp_total, gamemode, group_name, last_ip, "
    "inventory, ender_chest, stats, tags, post_effects";

/// A tag cannot contain a comma: vanilla's own tag argument is a single word.
[[nodiscard]] std::set<std::string, std::less<>> splitTags(const std::optional<std::string>& stored) {
    std::set<std::string, std::less<>> tags;
    if (!stored.has_value()) {
        return tags;
    }
    std::string current;
    for (const char character : *stored) {
        if (character == ',') {
            if (!current.empty()) {
                tags.insert(current);
            }
            current.clear();
            continue;
        }
        current += character;
    }
    if (!current.empty()) {
        tags.insert(current);
    }
    return tags;
}

[[nodiscard]] std::optional<std::string> joinTags(const std::set<std::string, std::less<>>& tags) {
    if (tags.empty()) {
        return std::nullopt;
    }
    std::string joined;
    for (const std::string& tag : tags) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += tag;
    }
    return joined;
}

/// An identifier cannot contain a comma either.
[[nodiscard]] std::vector<std::string> splitList(const std::optional<std::string>& stored) {
    std::vector<std::string> items;
    if (!stored.has_value()) {
        return items;
    }
    std::string_view rest = *stored;
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        if (const std::string_view item = rest.substr(0, comma); !item.empty()) {
            items.emplace_back(item);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(comma + 1);
    }
    return items;
}

[[nodiscard]] std::optional<std::string> joinList(const std::vector<std::string>& items) {
    if (items.empty()) {
        return std::nullopt;
    }
    std::string joined;
    for (const std::string& item : items) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += item;
    }
    return joined;
}

[[nodiscard]] StoredInventory inventoryOf(const std::vector<std::uint8_t>& blob, std::int32_t fallbackSize) {
    if (blob.empty()) {
        return StoredInventory::empty(fallbackSize);
    }
    const std::expected<StoredInventory, StorageFormatError> decoded = inventoryNbt::decode(blob);
    return decoded.has_value() ? *decoded : StoredInventory::empty(fallbackSize);
}

[[nodiscard]] StoredPlayer readRow(const sql::Statement& row) {
    StoredPlayer player;
    player.uuid = row.text(0);
    player.name = row.text(1);
    player.lastSeenEpochSeconds = row.integer(2);
    player.firstSeenEpochSeconds = row.integer(3);
    player.playTimeTicks = row.integer(4);
    player.world = row.text(5);
    player.x = row.real(6);
    player.y = row.real(7);
    player.z = row.real(8);
    player.yaw = static_cast<float>(row.real(9));
    player.pitch = static_cast<float>(row.real(10));
    player.health = static_cast<float>(row.real(11));
    player.food = static_cast<std::int32_t>(row.integer(12));
    player.saturation = static_cast<float>(row.real(13));
    player.exhaustion = static_cast<float>(row.real(14));
    player.xpLevel = static_cast<std::int32_t>(row.integer(15));
    player.xpProgress = static_cast<float>(row.real(16));
    player.xpTotal = static_cast<std::int32_t>(row.integer(17));
    player.gamemode = gamemodeById(static_cast<std::int32_t>(row.integer(18)));
    player.group = row.textOrNull(19);
    player.inventory = inventoryOf(row.blob(21), StoredInventory::kPlayerSlots);
    player.enderChest = inventoryOf(row.blob(22), StoredPlayer::kEnderChestSlots);
    const std::vector<std::uint8_t> statsBlob = row.blob(23);
    if (!statsBlob.empty()) {
        if (const std::expected<StoredStats, StorageFormatError> stats = statsNbt::decode(statsBlob);
            stats.has_value()) {
            player.stats = *stats;
        }
    }
    player.tags = splitTags(row.textOrNull(24));
    player.postEffects = splitList(row.textOrNull(25));
    return player;
}

[[nodiscard]] std::vector<PlayerSummary> summaries(sql::Statement& statement) {
    std::vector<PlayerSummary> found;
    while (statement.step()) {
        found.push_back(PlayerSummary{.uuid = statement.text(0),
                                      .name = statement.text(1),
                                      .value = statement.isNull(2) ? 0 : statement.integer(2)});
    }
    return found;
}

} // namespace

std::optional<StoredPlayer> PlayerRepository::loadOne(std::string_view sql, std::string_view key) {
    sql::Statement statement = database_->prepare(sql);
    statement.bind(1, key);
    if (!statement.step()) {
        return std::nullopt;
    }
    return readRow(statement);
}

std::optional<StoredPlayer> PlayerRepository::load(std::string_view uuid) {
    return loadOne("SELECT " + std::string{kColumns} + " FROM player WHERE uuid = ?", uuid);
}

std::optional<StoredPlayer> PlayerRepository::loadByName(std::string_view name) {
    // A name is not unique and never was: the same person has one row per UUID they have joined
    // with, and a server that has run in both online and offline mode has two for everybody. Most
    // recently seen wins, so `op <name>` acts on whoever that name last belonged to instead of
    // failing outright.
    return loadOne("SELECT " + std::string{kColumns} +
                       " FROM player WHERE name = ? COLLATE NOCASE ORDER BY last_seen DESC LIMIT 1",
                   name);
}

bool PlayerRepository::exists(std::string_view uuid) {
    sql::Statement statement = database_->prepare("SELECT EXISTS(SELECT 1 FROM player WHERE uuid = ?)");
    statement.bind(1, uuid);
    return statement.step() && statement.integer(0) != 0;
}

void PlayerRepository::save(const StoredPlayer& player) {
    sql::Statement statement = database_->prepare(R"(
INSERT INTO player(
    uuid, name, last_seen, first_seen, play_time_ticks, world, x, y, z, yaw, pitch,
    health, food, saturation, exhaustion, xp_level, xp_progress, xp_total, gamemode,
    group_name, inventory, ender_chest, stats, tags, post_effects
)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(uuid) DO UPDATE SET
    name = excluded.name,
    last_seen = excluded.last_seen,
    play_time_ticks = excluded.play_time_ticks,
    world = excluded.world,
    x = excluded.x, y = excluded.y, z = excluded.z,
    yaw = excluded.yaw, pitch = excluded.pitch,
    health = excluded.health,
    food = excluded.food,
    saturation = excluded.saturation,
    exhaustion = excluded.exhaustion,
    xp_level = excluded.xp_level,
    xp_progress = excluded.xp_progress,
    xp_total = excluded.xp_total,
    gamemode = excluded.gamemode,
    group_name = excluded.group_name,
    inventory = excluded.inventory,
    ender_chest = excluded.ender_chest,
    stats = excluded.stats,
    tags = excluded.tags,
    post_effects = excluded.post_effects)");
    const std::vector<std::uint8_t> inventory = inventoryNbt::encode(player.inventory);
    const std::vector<std::uint8_t> enderChest = inventoryNbt::encode(player.enderChest);
    const std::vector<std::uint8_t> stats = statsNbt::encode(player.stats);
    statement.bind(1, player.uuid);
    statement.bind(2, player.name);
    statement.bind(3, player.lastSeenEpochSeconds);
    statement.bind(4, player.firstSeenEpochSeconds);
    statement.bind(5, player.playTimeTicks);
    statement.bind(6, player.world);
    statement.bind(7, player.x);
    statement.bind(8, player.y);
    statement.bind(9, player.z);
    statement.bind(10, static_cast<double>(player.yaw));
    statement.bind(11, static_cast<double>(player.pitch));
    statement.bind(12, static_cast<double>(player.health));
    statement.bind(13, static_cast<std::int64_t>(player.food));
    statement.bind(14, static_cast<double>(player.saturation));
    statement.bind(15, static_cast<double>(player.exhaustion));
    statement.bind(16, static_cast<std::int64_t>(player.xpLevel));
    statement.bind(17, static_cast<double>(player.xpProgress));
    statement.bind(18, static_cast<std::int64_t>(player.xpTotal));
    statement.bind(19, static_cast<std::int64_t>(player.gamemode));
    statement.bind(20, player.group);
    statement.bind(21, std::span<const std::uint8_t>{inventory});
    statement.bind(22, std::span<const std::uint8_t>{enderChest});
    statement.bindBlobOrNull(23, stats);
    statement.bind(24, joinTags(player.tags));
    statement.bind(25, joinList(player.postEffects));
    statement.execute();
}

void PlayerRepository::saveAll(const std::vector<StoredPlayer>& players) {
    if (players.empty()) {
        return;
    }
    database_->transaction([this, &players] {
        for (const StoredPlayer& player : players) {
            save(player);
        }
        return true;
    });
}

bool PlayerRepository::setGroup(std::string_view uuid, const std::optional<std::string>& group) {
    if (!exists(uuid)) {
        return false;
    }
    sql::Statement statement = database_->prepare("UPDATE player SET group_name = ? WHERE uuid = ?");
    statement.bind(1, group);
    statement.bind(2, uuid);
    return statement.execute();
}

void PlayerRepository::setLastIp(std::string_view uuid, const std::optional<std::string>& address) {
    sql::Statement statement = database_->prepare("UPDATE player SET last_ip = ? WHERE uuid = ?");
    statement.bind(1, address);
    statement.bind(2, uuid);
    statement.execute();
}

std::optional<std::string> PlayerRepository::findNameByAddress(std::string_view address) {
    // Most recently seen wins: a household shares one address, and the last person to play on it is
    // the best guess available. A guess is all this is ever used for.
    sql::Statement statement =
        database_->prepare("SELECT name FROM player WHERE last_ip = ? ORDER BY last_seen DESC LIMIT 1");
    statement.bind(1, address);
    if (!statement.step()) {
        return std::nullopt;
    }
    return statement.text(0);
}

std::vector<PlayerSummary> PlayerRepository::findInGroup(std::string_view group) {
    sql::Statement statement =
        database_->prepare("SELECT uuid, name, NULL FROM player WHERE group_name = ? ORDER BY name");
    statement.bind(1, group);
    return summaries(statement);
}

void PlayerRepository::remove(std::string_view uuid) {
    sql::Statement statement = database_->prepare("DELETE FROM player WHERE uuid = ?");
    statement.bind(1, uuid);
    statement.execute();
}

std::int64_t PlayerRepository::count() {
    sql::Statement statement = database_->prepare("SELECT count(*) FROM player");
    return statement.step() ? statement.integer(0) : 0;
}

std::map<StoredGamemode, std::int64_t> PlayerRepository::countByGamemode() {
    std::map<StoredGamemode, std::int64_t> counts;
    sql::Statement statement = database_->prepare("SELECT gamemode, count(*) AS total FROM player GROUP BY gamemode");
    while (statement.step()) {
        counts[gamemodeById(static_cast<std::int32_t>(statement.integer(0)))] += statement.integer(1);
    }
    return counts;
}

std::vector<PlayerSummary> PlayerRepository::findLastSeenBefore(std::int64_t epochSeconds) {
    sql::Statement statement =
        database_->prepare("SELECT uuid, name, last_seen FROM player WHERE last_seen < ? ORDER BY last_seen");
    statement.bind(1, epochSeconds);
    return summaries(statement);
}

std::vector<PlayerSummary> PlayerRepository::findInWorld(std::string_view world) {
    sql::Statement statement = database_->prepare("SELECT uuid, name, NULL FROM player WHERE world = ? ORDER BY name");
    statement.bind(1, world);
    return summaries(statement);
}

std::vector<PlayerSummary> PlayerRepository::findWithPlayTimeAtLeast(std::int64_t ticks) {
    sql::Statement statement = database_->prepare(
        "SELECT uuid, name, play_time_ticks FROM player WHERE play_time_ticks >= ? ORDER BY play_time_ticks DESC");
    statement.bind(1, ticks);
    return summaries(statement);
}

} // namespace basalt::server::storage

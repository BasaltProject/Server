#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/storage/PlayerStore.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace basalt::server::storage;

namespace {

[[nodiscard]] std::string databasePath() {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    return (directory / "oracle_players.db").string();
}

void removeDatabase(const std::string& path) {
    std::error_code ignored;
    for (const std::string_view suffix : {"", "-wal", "-shm"}) {
        std::filesystem::remove(path + std::string{suffix}, ignored);
    }
}

/// Kotlin prints a Double and a Float with a decimal point always present.
[[nodiscard]] std::string number(double value) {
    std::string text = std::format("{}", value);
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos) {
        text += ".0";
    }
    return text;
}

[[nodiscard]] StoredPlayer player(std::string uuid, std::string name, std::string world = "overworld",
                                  StoredGamemode gamemode = StoredGamemode::Survival,
                                  std::int64_t lastSeen = 1700000000LL, std::int64_t playTime = 72000LL) {
    StoredPlayer built;
    built.uuid = std::move(uuid);
    built.name = std::move(name);
    built.lastSeenEpochSeconds = lastSeen;
    built.firstSeenEpochSeconds = 1600000000LL;
    built.playTimeTicks = playTime;
    built.world = std::move(world);
    built.x = 8.5;
    built.y = 63.0;
    built.z = -12.25;
    built.yaw = 90.0F;
    built.pitch = -12.5F;
    built.health = 17.5F;
    built.food = 18;
    built.saturation = 3.5F;
    built.exhaustion = 1.25F;
    built.xpLevel = 7;
    built.xpProgress = 0.4F;
    built.xpTotal = 62;
    built.gamemode = gamemode;
    return built;
}

[[nodiscard]] std::string_view modeName(StoredGamemode mode) {
    static constexpr std::string_view kNames[] = {"SURVIVAL", "CREATIVE", "ADVENTURE", "SPECTATOR"};
    return kNames[static_cast<std::size_t>(mode)];
}

/// The oracle prints a Kotlin `List.toString`, which is `[a, b]` and `[]` when empty.
[[nodiscard]] std::string bracketed(const std::set<std::string, std::less<>>& values) {
    std::string text = "[";
    for (const std::string& value : values) {
        if (text.size() > 1) {
            text += ", ";
        }
        text += value;
    }
    return text + "]";
}

[[nodiscard]] std::string describe(const std::optional<StoredPlayer>& player) {
    if (!player.has_value()) {
        return "-";
    }
    std::string slots;
    for (std::size_t index = 0; index < player->inventory.slots.size(); ++index) {
        const std::optional<StoredItemStack>& stack = player->inventory.slots[index];
        if (!stack.has_value()) {
            continue;
        }
        if (!slots.empty()) {
            slots += '|';
        }
        slots += std::to_string(index) + ":" + stack->id + "x" + std::to_string(stack->count);
    }
    std::string counters;
    for (const auto& [key, value] : player->stats.counters) {
        if (!counters.empty()) {
            counters += ',';
        }
        counters += key + "=" + std::to_string(value);
    }
    return player->uuid + "/" + player->name + "/" + player->world + "/" + std::string{modeName(player->gamemode)} +
           "/" + number(player->x) + "/" + number(static_cast<double>(player->yaw)) + "/" +
           number(static_cast<double>(player->health)) + "/" + std::to_string(player->food) + "/" +
           std::to_string(player->xpLevel) + "/" + std::to_string(player->playTimeTicks) + "/" +
           (player->group.has_value() ? *player->group : "null") + "/" + bracketed(player->tags) + "/" + counters +
           "/" + slots + "/" + std::to_string(player->enderChest.slots.size());
}

[[nodiscard]] std::string summaries(const std::vector<PlayerSummary>& found) {
    std::string text;
    for (const PlayerSummary& summary : found) {
        if (!text.empty()) {
            text += ',';
        }
        text += summary.name + "=" + std::to_string(summary.value);
    }
    return text;
}

} // namespace

TEST_CASE("the player database answers what the frozen kotlin answers") {
    const std::string path = databasePath();
    removeDatabase(path);

    {
        const std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> opened = PlayerStore::open(path);
        REQUIRE(opened.has_value());
        PlayerStore& store = **opened;
        PlayerRepository& players = store.players();

        StoredInventory inventory = StoredInventory::empty();
        inventory.slots[0] = StoredItemStack{.id = "minecraft:stone", .count = 64, .components = {}};
        basalt::wire::nbt::NbtCompound named;
        named.putString("minecraft:custom_name", "Biter");
        inventory.slots[8] =
            StoredItemStack{.id = "minecraft:diamond_sword", .count = 1, .components = std::move(named)};

        StoredPlayer whiron = player("11111111-2222-3333-4444-555555555555", "Whiron");
        whiron.inventory = std::move(inventory);
        whiron.tags = {"builder", "alpha"};
        whiron.postEffects = {"minecraft:shaders/post/invert", "minecraft:shaders/post/blur"};
        whiron.stats = StoredStats{}.with(stats::deaths(), 4).with(stats::jump(), 900);
        players.save(whiron);
        players.save(player("22222222-0000-0000-0000-000000000000", "Ana", "nether", StoredGamemode::Survival,
                            1700000000LL, 10LL));
        players.save(player("33333333-0000-0000-0000-000000000000", "Bo", "overworld", StoredGamemode::Creative,
                            1500000000LL, 500000LL));
        // The same name under a second UUID, seen more recently: the name lookup takes this one.
        players.save(player("44444444-0000-0000-0000-000000000000", "whiron", "the_end", StoredGamemode::Survival,
                            1800000000LL));

        CHECK(describe(players.load("11111111-2222-3333-4444-555555555555")) ==
              "11111111-2222-3333-4444-555555555555/Whiron/overworld/SURVIVAL/8.5/90.0/17.5/18/7/72000/null/"
              "[alpha, builder]/minecraft:custom/minecraft:deaths=4,minecraft:custom/minecraft:jump=900/"
              "0:minecraft:stonex64|8:minecraft:diamond_swordx1/27");
        CHECK(describe(players.loadByName("WHIRON")) ==
              "44444444-0000-0000-0000-000000000000/whiron/the_end/SURVIVAL/8.5/90.0/17.5/18/7/72000/null/[]///27");
        CHECK(describe(players.load("nobody")) == "-");
        // In the order they were added, and nothing for someone who has none.
        CHECK(players.load("11111111-2222-3333-4444-555555555555")->postEffects == whiron.postEffects);
        CHECK(players.load("33333333-0000-0000-0000-000000000000")->postEffects.empty());
        // The one player who is not in survival, so the mode really comes off the row.
        CHECK(describe(players.load("33333333-0000-0000-0000-000000000000")) ==
              "33333333-0000-0000-0000-000000000000/Bo/overworld/CREATIVE/8.5/90.0/17.5/18/7/500000/null/[]///27");

        // Empty stats are a null column, not a zero-length blob: what the frozen Kotlin's own
        // encoder answers with, and what `sqlite3` shows an operator looking at the file.
        {
            sql::Statement column =
                store.database().prepare("SELECT stats IS NULL, typeof(inventory) FROM player WHERE uuid = ?");
            column.bind(1, std::string_view{"33333333-0000-0000-0000-000000000000"});
            REQUIRE(column.step());
            CHECK(column.integer(0) == 1);
            CHECK(column.text(1) == "blob");
        }
        // Journal mode is this build's own configuration rather than anything the oracle can see,
        // and it is the reason one file is safe to write while it is being read.
        {
            sql::Statement mode = store.database().prepare("PRAGMA journal_mode");
            REQUIRE(mode.step());
            CHECK(mode.text(0) == "wal");
        }

        CHECK(players.count() == 4);
        CHECK_FALSE(players.exists("nobody"));
        std::string modes;
        for (const auto& [mode, total] : players.countByGamemode()) {
            if (!modes.empty()) {
                modes += ',';
            }
            modes += std::string{modeName(mode)} + "=" + std::to_string(total);
        }
        // The oracle sorts by name, which puts creative before survival.
        CHECK(modes == "SURVIVAL=3,CREATIVE=1");

        CHECK(summaries(players.findLastSeenBefore(1700000000LL)) == "Bo=1500000000");
        CHECK(summaries(players.findInWorld("nether")) == "Ana=0");
        CHECK(summaries(players.findWithPlayTimeAtLeast(72000LL)) == "Bo=500000,Whiron=72000,whiron=72000");

        CHECK(players.setGroup("11111111-2222-3333-4444-555555555555", "admin"));
        CHECK_FALSE(players.setGroup("nobody", "admin"));
        CHECK(summaries(players.findInGroup("admin")) == "Whiron=0");

        players.setLastIp("11111111-2222-3333-4444-555555555555", "203.0.113.7");
        players.setLastIp("22222222-0000-0000-0000-000000000000", "203.0.113.7");
        CHECK(players.findNameByAddress("203.0.113.7") == "Whiron");
        CHECK_FALSE(players.findNameByAddress("198.51.100.1").has_value());

        // A save that overwrites: the group survives it, because the upsert does not carry one.
        players.saveAll({player("22222222-0000-0000-0000-000000000000", "Ana", "overworld", StoredGamemode::Survival,
                                1700000000LL, 99LL),
                         player("55555555-0000-0000-0000-000000000000", "Cy")});
        CHECK(players.count() == 5);
        CHECK(describe(players.load("22222222-0000-0000-0000-000000000000")) ==
              "22222222-0000-0000-0000-000000000000/Ana/overworld/SURVIVAL/8.5/90.0/17.5/18/7/99/null/[]///27");
        players.remove("55555555-0000-0000-0000-000000000000");
        CHECK(players.count() == 4);

        AccessRepository& access = store.access();
        access.ban(BanRecord{.subject = "11111111-2222-3333-4444-555555555555",
                             .kind = BanKind::Player,
                             .name = "Whiron",
                             .reason = "griefing",
                             .source = "console",
                             .createdAtEpochSeconds = 1700000100LL,
                             .expiresAtEpochSeconds = std::nullopt});
        access.ban(BanRecord{.subject = "203.0.113.9",
                             .kind = BanKind::Ip,
                             .name = std::nullopt,
                             .reason = "spam",
                             .source = "Ana",
                             .createdAtEpochSeconds = 1700000200LL,
                             .expiresAtEpochSeconds = 1800000000LL});
        // Created later but sorting earlier by subject, so the order is the created one.
        access.ban(BanRecord{.subject = "198.51.100.4",
                             .kind = BanKind::Ip,
                             .name = std::nullopt,
                             .reason = "bot",
                             .source = "Bo",
                             .createdAtEpochSeconds = 1700000300LL,
                             .expiresAtEpochSeconds = std::nullopt});
        // The text that goes in the column, which is the whole of the two kinds not colliding.
        CHECK(storedNameOf(BanKind::Player) == "player");
        CHECK(storedNameOf(BanKind::Ip) == "ip");
        const std::optional<BanRecord> ban = access.banOf("11111111-2222-3333-4444-555555555555", BanKind::Player);
        REQUIRE(ban.has_value());
        CHECK(ban->reason == "griefing");
        CHECK(ban->source == "console");
        CHECK(ban->name == "Whiron");
        CHECK_FALSE(ban->expiresAtEpochSeconds.has_value());
        const std::vector<BanRecord> ipBans = access.bans(BanKind::Ip);
        REQUIRE(ipBans.size() == 2);
        CHECK(ipBans[0].subject == "203.0.113.9");
        CHECK(ipBans[0].expiresAtEpochSeconds == 1800000000LL);
        CHECK(ipBans[1].subject == "198.51.100.4");
        CHECK_FALSE(ipBans[1].expiresAtEpochSeconds.has_value());
        CHECK_FALSE(access.banOf("11111111-2222-3333-4444-555555555555", BanKind::Ip).has_value());
        CHECK(access.pardon("203.0.113.9", BanKind::Ip));
        CHECK_FALSE(access.pardon("203.0.113.9", BanKind::Ip));

        access.addToWhitelist(WhitelistEntry{
            .uuid = "11111111-2222-3333-4444-555555555555", .name = "Whiron", .addedAtEpochSeconds = 1700000300LL});
        access.addToWhitelist(WhitelistEntry{
            .uuid = "22222222-0000-0000-0000-000000000000", .name = "Ana", .addedAtEpochSeconds = 1700000400LL});
        std::string listed;
        for (const WhitelistEntry& entry : access.whitelist()) {
            if (!listed.empty()) {
                listed += ',';
            }
            listed += entry.name + "@" + std::to_string(entry.addedAtEpochSeconds);
        }
        CHECK(listed == "Ana@1700000400,Whiron@1700000300");
        CHECK(access.isWhitelisted("11111111-2222-3333-4444-555555555555"));
        CHECK_FALSE(access.isWhitelisted("nobody"));
        CHECK(access.whitelistedByName("ANA")->uuid == "22222222-0000-0000-0000-000000000000");
        CHECK(access.removeFromWhitelist("22222222-0000-0000-0000-000000000000"));
        CHECK_FALSE(access.removeFromWhitelist("22222222-0000-0000-0000-000000000000"));

        access.setSpawnPoint(StoredSpawnPoint{.uuid = "11111111-2222-3333-4444-555555555555",
                                              .world = "overworld",
                                              .x = 100.5,
                                              .y = 64.0,
                                              .z = -200.25,
                                              .yaw = 45.0F});
        const std::optional<StoredSpawnPoint> spawn = access.spawnPointOf("11111111-2222-3333-4444-555555555555");
        REQUIRE(spawn.has_value());
        CHECK(spawn->world + "/" + number(spawn->x) + "/" + number(spawn->y) + "/" + number(spawn->z) + "/" +
                  number(static_cast<double>(spawn->yaw)) ==
              "overworld/100.5/64.0/-200.25/45.0");
        CHECK_FALSE(access.spawnPointOf("nobody").has_value());
        access.clearSpawnPoint("11111111-2222-3333-4444-555555555555");
        CHECK_FALSE(access.spawnPointOf("11111111-2222-3333-4444-555555555555").has_value());

        ProgressRepository& progress = store.progress();
        progress.grantCriterion("uuid-a", "minecraft:story/root", "crafting_table");
        progress.grantCriterion("uuid-a", "minecraft:story/root", "planks");
        // Granting twice is one row, not two.
        progress.grantCriterion("uuid-a", "minecraft:story/root", "planks");
        progress.grantCriterion("uuid-a", "minecraft:story/mine_stone", "get_stone");
        progress.grantCriterion("uuid-b", "minecraft:story/root", "crafting_table");
        progress.learnRecipe("uuid-a", "minecraft:torch");
        progress.learnRecipe("uuid-a", "minecraft:stick");
        progress.learnRecipe("uuid-a", "minecraft:stick");
        const auto describeProgress = [](const StoredProgress& loaded) {
            std::string text;
            for (const auto& [advancement, criteria] : loaded.criteria) {
                if (!text.empty()) {
                    text += ';';
                }
                text += advancement + "=" + bracketed(criteria);
            }
            return text + " " + bracketed(loaded.recipes);
        };
        CHECK(describeProgress(progress.load("uuid-a")) ==
              "minecraft:story/mine_stone=[get_stone];minecraft:story/root=[crafting_table, planks] "
              "[minecraft:stick, minecraft:torch]");
        progress.revokeCriterion("uuid-a", "minecraft:story/root", "planks");
        progress.revokeAdvancement("uuid-a", "minecraft:story/mine_stone");
        progress.forgetRecipe("uuid-a", "minecraft:stick");
        CHECK(describeProgress(progress.load("uuid-a")) == "minecraft:story/root=[crafting_table] [minecraft:torch]");
        CHECK(progress.load("uuid-b").criteria.size() == 1);
        CHECK(progress.load("uuid-b").criteria.contains("minecraft:story/root"));

        PluginDataRepository& data = store.pluginData();
        data.set("shop", "uuid-a", "balance", "125.5");
        data.set("shop", "uuid-b", "balance", "980");
        data.set("shop", "uuid-c", "balance", "7");
        data.set("shop", "uuid-a", "title", "Merchant");
        // A different plugin, same key and player: neither can see the other's row.
        data.set("duels", "uuid-a", "balance", "3");
        CHECK(data.get("shop", "uuid-a", "balance") == "125.5");
        CHECK(data.get("duels", "uuid-a", "balance") == "3");
        CHECK_FALSE(data.get("shop", "uuid-a", "nothing").has_value());
        std::string all;
        for (const auto& [key, value] : data.all("shop", "uuid-a")) {
            if (!all.empty()) {
                all += ',';
            }
            all += key + "=" + value;
        }
        CHECK(all == "balance=125.5,title=Merchant");
        std::string top;
        for (const auto& [uuid, value] : data.top("shop", "balance", 2)) {
            if (!top.empty()) {
                top += ',';
            }
            top += uuid + "=" + value;
        }
        CHECK(top == "uuid-b=980,uuid-a=125.5");
        const auto describeNamespaces = [&data] {
            std::string text;
            for (const auto& [plugin, rows] : data.namespaces()) {
                if (!text.empty()) {
                    text += ',';
                }
                text += plugin + "=" + std::to_string(rows);
            }
            return text;
        };
        CHECK(describeNamespaces() == "duels=1,shop=4");
        data.set("shop", "uuid-a", "title", std::nullopt);
        data.purge("duels");
        CHECK_FALSE(data.get("shop", "uuid-a", "title").has_value());
        CHECK(describeNamespaces() == "shop=3");

        CHECK_FALSE(store.database().hasError());
    }

    // The checkpoint folded the log back in, so the file alone is the database: backing it up
    // without its -wal copies everything.
    CHECK(std::filesystem::file_size(path) > 4096);
    CHECK_FALSE(std::filesystem::exists(path + "-wal"));

    // Reopening reads back what the checkpoint folded in.
    {
        const std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError> reopened = PlayerStore::open(path);
        REQUIRE(reopened.has_value());
        CHECK((*reopened)->players().count() == 4);
        CHECK(describe((*reopened)->players().load("11111111-2222-3333-4444-555555555555")) ==
              "11111111-2222-3333-4444-555555555555/Whiron/overworld/SURVIVAL/8.5/90.0/17.5/18/7/72000/admin/"
              "[alpha, builder]/minecraft:custom/minecraft:deaths=4,minecraft:custom/minecraft:jump=900/"
              "0:minecraft:stonex64|8:minecraft:diamond_swordx1/27");
    }

    removeDatabase(path);
}

TEST_CASE("a database missing a column this build needs is refused, not used") {
    const std::filesystem::path directory{BASALT_TEST_SCRATCH_DIR};
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    const std::string path = (directory / "old_schema.db").string();
    removeDatabase(path);

    {
        // The whole schema, then one column taken back off `player`: every other table is current,
        // so what the message has to name is the one that is a build behind.
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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::RealLootTables;
using basalt::server::world::loot::ItemStack;
using basalt::server::world::loot::LootTable;
using basalt::wire::common::Identifier;

namespace {

constexpr std::string_view kCobblestoneTable =
    R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:cobblestone"}]}]})";

/// A jar that is a map of names to text, remembering every name that was asked for.
struct FakeJar {
    std::map<std::string, std::string> files;
    std::vector<std::string> requested;

    [[nodiscard]] RealLootTables::Jar handle() {
        return RealLootTables::Jar{
            .readText = [this](std::string_view name) -> std::optional<std::string> {
                requested.emplace_back(name);
                const auto found = files.find(std::string{name});
                return found == files.end() ? std::nullopt : std::optional{found->second};
            },
            .entriesUnder =
                [this](std::string_view prefix) {
                    std::vector<std::string> names;
                    for (const auto& [name, text] : files) {
                        if (name.starts_with(prefix)) {
                            names.push_back(name);
                        }
                    }
                    return names;
                },
        };
    }
};

[[nodiscard]] std::vector<ItemStack> roll(const LootTable& table) {
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(1234);
    return table.generate(random);
}

} // namespace

TEST_CASE("a block's own table is read once and rolls what it names") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/blocks/stone.json"] = std::string{kCobblestoneTable};
    RealLootTables tables{jar.handle()};

    const LootTable* stone = tables.forBlock(Identifier::vanilla("stone"));
    REQUIRE(stone != nullptr);
    CHECK(jar.requested == std::vector<std::string>{"data/minecraft/loot_table/blocks/stone.json"});

    const std::vector<ItemStack> rolled = roll(*stone);
    REQUIRE(rolled.size() == 1);
    CHECK(rolled[0].item == Identifier::vanilla("cobblestone"));
    CHECK(rolled[0].count == 1);

    // Cached: the same table comes back and the jar is not read a second time.
    CHECK(tables.forBlock(Identifier::vanilla("stone")) == stone);
    CHECK(jar.requested.size() == 1);
}

TEST_CASE("a table this build cannot parse is remembered as nothing") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/blocks/dirt.json"] =
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:loot_table","name":"minecraft:empty"}]}]})";
    RealLootTables tables{jar.handle()};

    CHECK(tables.forBlock(Identifier::vanilla("dirt")) == nullptr);
    CHECK(tables.forBlock(Identifier::vanilla("dirt")) == nullptr);
    CHECK(jar.requested.size() == 1);
}

TEST_CASE("a table the jar does not ship is remembered as nothing too") {
    FakeJar jar;
    RealLootTables tables{jar.handle()};

    CHECK(tables.forBlock(Identifier::vanilla("air")) == nullptr);
    CHECK(tables.forBlock(Identifier::vanilla("air")) == nullptr);
    CHECK(jar.requested == std::vector<std::string>{"data/minecraft/loot_table/blocks/air.json"});
}

TEST_CASE("a table by its own id is the whole path under the table directory") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/chests/simple_dungeon.json"] = std::string{kCobblestoneTable};
    RealLootTables tables{jar.handle()};

    CHECK(tables.byId(Identifier::vanilla("chests/simple_dungeon")) != nullptr);
    CHECK(jar.requested == std::vector<std::string>{"data/minecraft/loot_table/chests/simple_dungeon.json"});
    CHECK(tables.byId(Identifier::vanilla("chests/nothing_here")) == nullptr);
}

TEST_CASE("an entity's table is read and keyed under entities") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/entities/zombie.json"] = std::string{kCobblestoneTable};
    RealLootTables tables{jar.handle()};

    const LootTable* zombie = tables.forEntity(Identifier::vanilla("zombie"));
    REQUIRE(zombie != nullptr);
    CHECK(jar.requested == std::vector<std::string>{"data/minecraft/loot_table/entities/zombie.json"});

    // The cache key is `entities/<type>`, so the same table answers by that id with no second read.
    CHECK(tables.byId(Identifier::vanilla("entities/zombie")) == zombie);
    CHECK(jar.requested.size() == 1);
}

TEST_CASE("ids lists every json the table directory ships, and nothing else") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/blocks/stone.json"] = std::string{kCobblestoneTable};
    jar.files["data/minecraft/loot_table/entities/zombie.json"] = std::string{kCobblestoneTable};
    jar.files["data/minecraft/loot_table/blocks/stone.mcmeta"] = "{}";
    RealLootTables tables{jar.handle()};

    const std::vector<Identifier> ids = tables.ids();
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == Identifier::vanilla("blocks/stone"));
    CHECK(ids[1] == Identifier::vanilla("entities/zombie"));
}

TEST_CASE("without a jar nothing resolves and nothing is remembered") {
    RealLootTables tables{RealLootTables::Jar{}};

    CHECK(tables.forBlock(Identifier::vanilla("stone")) == nullptr);
    CHECK(tables.byId(Identifier::vanilla("chests/simple_dungeon")) == nullptr);
    CHECK(tables.forEntity(Identifier::vanilla("zombie")) == nullptr);
    CHECK(tables.ids().empty());
}

TEST_CASE("a wall-mounted block drops what the standing one drops") {
    FakeJar jar;
    jar.files["data/minecraft/loot_table/blocks/redstone_torch.json"] = std::string{kCobblestoneTable};
    RealLootTables tables{jar.handle()};

    // parity: `Blocks.wallVariant`. There is no `blocks/redstone_wall_torch.json` in the jar, so
    // asking for one is asking for nothing: a torch knocked off its wall has to roll the standing
    // block's table.
    REQUIRE(tables.forBlock(Identifier::vanilla("redstone_wall_torch")) != nullptr);
    CHECK(jar.requested.size() == 1);
    CHECK(jar.requested.front() == "data/minecraft/loot_table/blocks/redstone_torch.json");
    // The same table, cached under the standing name, so the pair costs one read between them.
    CHECK(tables.forBlock(Identifier::vanilla("redstone_torch")) ==
          tables.forBlock(Identifier::vanilla("redstone_wall_torch")));
    CHECK(jar.requested.size() == 1);

    // A block whose name merely ends in a wall is not one of them.
    CHECK(tables.forBlock(Identifier::vanilla("cobblestone_wall")) == nullptr);
    CHECK(jar.requested.back() == "data/minecraft/loot_table/blocks/cobblestone_wall.json");
}

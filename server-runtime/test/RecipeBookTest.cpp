#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::RealRecipes;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::RecipeBookAddPacket;
using basalt::wire::protocol::play::RecipeBookEntry;
using basalt::wire::protocol::play::RecipeBookRemovePacket;
using basalt::wire::protocol::play::SlotDisplay;
namespace book = basalt::server::runtime::world::recipeBook;
namespace reg = basalt::server::registry;

namespace {

struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealRecipes::Jar handle() {
        return RealRecipes::Jar{
            .readText = [this](std::string_view name) -> std::optional<std::string> {
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

/// Everything a session needs, plus the packets it was sent.
struct Sent {
    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> packets;

    [[nodiscard]] PlayerConnection connection() {
        PlayerConnection connection;
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            const std::span<const std::uint8_t> written = buffer.readable();
            packets.emplace_back(id, std::vector<std::uint8_t>{written.begin(), written.end()});
        };
        return connection;
    }
};

[[nodiscard]] std::int32_t itemId(const std::string& name) {
    const reg::Item* item = reg::items::registry().byName(Identifier::parse(name).value());
    REQUIRE(item != nullptr);
    return item->protocolId;
}

[[nodiscard]] std::string recipePath(const std::string& path) { return "data/minecraft/recipe/" + path + ".json"; }

[[nodiscard]] FakeJar twoRecipes() {
    FakeJar jar;
    jar.files[recipePath("torch")] = R"({
        "type": "minecraft:crafting_shapeless",
        "ingredients": ["minecraft:coal", "minecraft:stick"],
        "result": {"id": "minecraft:torch", "count": 4}
    })";
    jar.files[recipePath("chest")] = R"({
        "type": "minecraft:crafting_shaped",
        "pattern": ["###", "# #", "###"],
        "key": {"#": "minecraft:oak_planks"},
        "result": {"id": "minecraft:chest", "count": 1}
    })";
    jar.files[recipePath("iron_ingot_from_smelting")] = R"({
        "type": "minecraft:smelting",
        "ingredient": "minecraft:raw_iron",
        "result": {"id": "minecraft:iron_ingot"},
        "experience": 0.7,
        "cookingtime": 200
    })";
    return jar;
}

} // namespace

TEST_CASE("a shapeless recipe is drawn as its ingredients and its result") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};

    const std::optional<RecipeBookEntry> entry = book::entryFor(recipes, Identifier::vanilla("torch"));

    REQUIRE(entry.has_value());
    CHECK_FALSE(entry->display.grid.has_value());
    REQUIRE(entry->display.ingredients.size() == 2);
    REQUIRE(entry->display.result.as<SlotDisplay::Item>() != nullptr);
    CHECK(entry->display.result.as<SlotDisplay::Item>()->itemId == itemId("minecraft:torch"));
    CHECK(entry->display.craftingStation.as<SlotDisplay::Item>()->itemId == itemId("minecraft:crafting_table"));
    CHECK(entry->category == RecipeBookEntry::kCraftingMiscCategory);
    REQUIRE(entry->craftingRequirements.size() == 2);
    CHECK(entry->craftingRequirements[0].size() == 1);
}

TEST_CASE("a shaped recipe carries its grid and its empty cells") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};

    const std::optional<RecipeBookEntry> entry = book::entryFor(recipes, Identifier::vanilla("chest"));

    REQUIRE(entry.has_value());
    REQUIRE(entry->display.grid.has_value());
    CHECK(entry->display.grid->width == 3);
    CHECK(entry->display.grid->height == 3);
    REQUIRE(entry->display.ingredients.size() == 9);
    // The hole in the middle is an empty slot, and it asks for nothing.
    CHECK(entry->display.ingredients[4].as<SlotDisplay::Empty>() != nullptr);
    CHECK(entry->craftingRequirements.size() == 8);
}

TEST_CASE("a recipe the book does not draw is left out of it") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};

    // parity: the furnace tabs come with the furnace menu rather than the crafting book.
    CHECK_FALSE(book::entryFor(recipes, Identifier::vanilla("iron_ingot_from_smelting")).has_value());
    CHECK_FALSE(book::entryFor(recipes, Identifier::vanilla("nothing_of_the_sort")).has_value());
}

TEST_CASE("adding recipes sends one packet with the ones that could be drawn") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};
    Sent sent;
    const PlayerSession player{Uuid{1, 1}, "Steve", 1, sent.connection()};

    book::add(
        player, recipes,
        {Identifier::vanilla("torch"), Identifier::vanilla("iron_ingot_from_smelting"), Identifier::vanilla("chest")});

    REQUIRE(sent.packets.size() == 1);
    CHECK(sent.packets[0].first == RecipeBookAddPacket::kId);
    basalt::wire::io::ByteBuffer buffer;
    buffer.writeBytes(sent.packets[0].second);
    const std::optional<RecipeBookAddPacket> packet = RecipeBookAddPacket::read(buffer);
    REQUIRE(packet.has_value());
    CHECK(packet->entries.size() == 2);
    CHECK_FALSE(packet->replace);
}

TEST_CASE("adding nothing the book can draw sends nothing at all") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};
    Sent sent;
    const PlayerSession player{Uuid{2, 2}, "Steve", 2, sent.connection()};

    book::add(player, recipes, {Identifier::vanilla("iron_ingot_from_smelting")});
    book::add(player, recipes, {});

    CHECK(sent.packets.empty());
}

TEST_CASE("removing recipes sends their numbers, skipping the ones nobody knows") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};
    Sent sent;
    const PlayerSession player{Uuid{3, 3}, "Steve", 3, sent.connection()};

    book::remove(player, recipes, {Identifier::vanilla("torch"), Identifier::vanilla("nothing_of_the_sort")});

    REQUIRE(sent.packets.size() == 1);
    CHECK(sent.packets[0].first == RecipeBookRemovePacket::kId);
    basalt::wire::io::ByteBuffer buffer;
    buffer.writeBytes(sent.packets[0].second);
    const std::optional<RecipeBookRemovePacket> packet = RecipeBookRemovePacket::read(buffer);
    REQUIRE(packet.has_value());
    CHECK(packet->recipeIds.size() == 1);

    // A recipe the book cannot draw still has a number, since the number is the recipe's own.
    sent.packets.clear();
    book::remove(player, recipes, {Identifier::vanilla("iron_ingot_from_smelting")});
    CHECK(sent.packets.size() == 1);

    sent.packets.clear();
    book::remove(player, recipes, {Identifier::vanilla("nothing_of_the_sort")});
    CHECK(sent.packets.empty());
}

TEST_CASE("a recipe's number is where it sits in the whole numbered list") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};

    const std::optional<RecipeBookEntry> chest = book::entryFor(recipes, Identifier::vanilla("chest"));
    const std::optional<RecipeBookEntry> torch = book::entryFor(recipes, Identifier::vanilla("torch"));

    REQUIRE(chest.has_value());
    REQUIRE(torch.has_value());
    // The list is in name order, so the chest comes before the smelting one and the torch after it.
    CHECK(chest->recipeId == 0);
    CHECK(torch->recipeId == 2);
}

TEST_CASE("a player with no connection behind them is sent nothing and is not an error") {
    FakeJar jar = twoRecipes();
    const RealRecipes recipes{jar.handle()};
    const PlayerSession player{Uuid{4, 4}, "Steve", 4, PlayerConnection{}};

    book::add(player, recipes, {Identifier::vanilla("torch")});
    book::remove(player, recipes, {Identifier::vanilla("torch")});
}

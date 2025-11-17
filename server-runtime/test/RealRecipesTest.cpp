#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>

using basalt::server::runtime::world::RealRecipes;
using basalt::wire::common::Identifier;
namespace crafting = basalt::server::world::crafting;
namespace loot = basalt::server::world::loot;

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
                    std::ranges::reverse(names);
                    return names;
                },
        };
    }
};

[[nodiscard]] std::string recipe(const std::string& path) { return "data/minecraft/recipe/" + path + ".json"; }

[[nodiscard]] std::string itemTag(const std::string& path) { return "data/minecraft/tags/item/" + path + ".json"; }

[[nodiscard]] loot::ItemStack stack(const std::string& item, std::int32_t count = 1) {
    return loot::ItemStack{.item = Identifier::parse(item).value(), .count = count};
}

[[nodiscard]] crafting::CraftingInput grid(std::int32_t width, std::int32_t height,
                                           std::vector<std::optional<loot::ItemStack>> items) {
    return crafting::CraftingInput::of(width, height, std::move(items));
}

constexpr std::string_view kShapeless = R"({
    "type": "minecraft:crafting_shapeless",
    "ingredients": ["minecraft:oak_planks", "minecraft:stick"],
    "result": {"id": "minecraft:torch", "count": 4}
})";

constexpr std::string_view kSmelting = R"({
    "type": "minecraft:smelting",
    "ingredient": "minecraft:iron_ore",
    "result": {"id": "minecraft:iron_ingot"},
    "experience": 0.7
})";

constexpr std::string_view kStonecutting = R"({
    "type": "minecraft:stonecutting",
    "ingredient": "minecraft:stone",
    "result": {"id": "minecraft:stone_slab", "count": 2}
})";

} // namespace

TEST_CASE("every recipe file is named, and only the ones this build understands are drawn") {
    FakeJar jar{.files = {{recipe("torch"), std::string{kShapeless}},
                          {recipe("iron_ingot"), std::string{kSmelting}},
                          {recipe("firework"), R"({"type": "minecraft:crafting_special_firework_rocket"})"},
                          {recipe("broken"), "{not json"}}};

    const RealRecipes recipes{jar.handle()};

    // Every file is named, whether or not it could be read.
    CHECK(recipes.ids().size() == 4);
    CHECK(recipes.ids().contains(Identifier::vanilla("firework")));
    CHECK(recipes.ids().contains(Identifier::vanilla("broken")));
    // But only the ones that parsed have a recipe behind them.
    CHECK(recipes.recipeFor(Identifier::vanilla("torch")) != nullptr);
    CHECK(recipes.recipeFor(Identifier::vanilla("broken")) == nullptr);
    CHECK(recipes.recipeFor(Identifier::vanilla("nothing_of_the_sort")) == nullptr);
}

TEST_CASE("a shapeless recipe answers with what it makes and the name it was read under") {
    FakeJar jar{.files = {{recipe("torch"), std::string{kShapeless}}}};
    const RealRecipes recipes{jar.handle()};

    const crafting::CraftingInput input = grid(2, 1, {stack("minecraft:stick"), stack("minecraft:oak_planks")});
    const std::optional<RealRecipes::Match> match = recipes.recipeMatching(input);

    REQUIRE(match.has_value());
    CHECK(match->id == Identifier::vanilla("torch"));
    const std::optional<loot::ItemStack> result = recipes.resultFor(input);
    REQUIRE(result.has_value());
    CHECK(result->item == Identifier::vanilla("torch"));
    CHECK(result->count == 4);

    // A grid that is not a recipe makes nothing at all.
    CHECK_FALSE(recipes.resultFor(grid(1, 1, {stack("minecraft:stick")})).has_value());
    CHECK_FALSE(recipes.recipeMatching(crafting::CraftingInput::empty()).has_value());
}

TEST_CASE("each of the four cookers only sees its own recipes") {
    FakeJar jar{.files = {{recipe("iron_ingot"), std::string{kSmelting}}, {recipe("iron_ingot_from_blasting"), R"({
                               "type": "minecraft:blasting",
                               "ingredient": "minecraft:iron_ore",
                               "result": {"id": "minecraft:iron_nugget"}
                           })"}}};
    const RealRecipes recipes{jar.handle()};

    const crafting::Recipe* smelted = recipes.cookingFor(crafting::Cooker::Smelting, stack("minecraft:iron_ore"));
    REQUIRE(smelted != nullptr);
    CHECK(smelted->result.item == Identifier::vanilla("iron_ingot"));
    CHECK(smelted->experience == doctest::Approx(0.7F));

    const crafting::Recipe* blasted = recipes.cookingFor(crafting::Cooker::Blasting, stack("minecraft:iron_ore"));
    REQUIRE(blasted != nullptr);
    CHECK(blasted->result.item == Identifier::vanilla("iron_nugget"));

    CHECK(recipes.cookingFor(crafting::Cooker::Smoking, stack("minecraft:iron_ore")) == nullptr);
    CHECK(recipes.cookingFor(crafting::Cooker::Smelting, stack("minecraft:gold_ore")) == nullptr);
}

TEST_CASE("the stonecutter keeps the order the files were read in") {
    FakeJar jar{.files = {{recipe("a_slab"), std::string{kStonecutting}},
                          {recipe("b_stairs"), R"({
                               "type": "minecraft:stonecutting",
                               "ingredient": "minecraft:stone",
                               "result": {"id": "minecraft:stone_stairs"}
                           })"},
                          {recipe("c_wall"), R"({
                               "type": "minecraft:stonecutting",
                               "ingredient": "minecraft:andesite",
                               "result": {"id": "minecraft:andesite_wall"}
                           })"}}};
    const RealRecipes recipes{jar.handle()};

    // The client sends back an index into this list, so both sides have to agree on the order.
    REQUIRE(recipes.stonecutting().size() == 3);
    CHECK(recipes.stonecutting()[0]->result.item == Identifier::vanilla("stone_slab"));
    CHECK(recipes.stonecutting()[1]->result.item == Identifier::vanilla("stone_stairs"));
    CHECK(recipes.stonecutting()[2]->result.item == Identifier::vanilla("andesite_wall"));

    const std::vector<const crafting::Recipe*> forStone = recipes.stonecuttingFor(stack("minecraft:stone"));
    REQUIRE(forStone.size() == 2);
    CHECK(forStone[0]->result.item == Identifier::vanilla("stone_slab"));
    CHECK(forStone[1]->result.item == Identifier::vanilla("stone_stairs"));
    CHECK(recipes.stonecuttingFor(stack("minecraft:dirt")).empty());
}

TEST_CASE("a smithing table walks its recipes in order too") {
    FakeJar jar{.files = {{recipe("netherite_sword"), R"({
        "type": "minecraft:smithing_transform",
        "template": "minecraft:netherite_upgrade_smithing_template",
        "base": "minecraft:diamond_sword",
        "addition": "minecraft:netherite_ingot",
        "result": {"id": "minecraft:netherite_sword"}
    })"}}};
    const RealRecipes recipes{jar.handle()};

    REQUIRE(recipes.smithing().size() == 1);
    const loot::ItemStack templateStack = stack("minecraft:netherite_upgrade_smithing_template");
    const loot::ItemStack base = stack("minecraft:diamond_sword");
    const loot::ItemStack addition = stack("minecraft:netherite_ingot");
    const crafting::Recipe* match = recipes.smithingFor(&templateStack, &base, &addition);
    REQUIRE(match != nullptr);
    CHECK(match->result.item == Identifier::vanilla("netherite_sword"));

    const loot::ItemStack wrong = stack("minecraft:iron_ingot");
    CHECK(recipes.smithingFor(&templateStack, &base, &wrong) == nullptr);
    CHECK(recipes.smithingFor(nullptr, &base, &addition) == nullptr);
}

TEST_CASE("an ingredient that names a tag matches everything the tag holds") {
    FakeJar jar{.files = {{itemTag("planks"), R"({"values": ["minecraft:oak_planks", "#minecraft:birch"]})"},
                          {itemTag("birch"), R"({"values": ["minecraft:birch_planks"]})"},
                          {recipe("stick"), R"({
                               "type": "minecraft:crafting_shapeless",
                               "ingredients": ["#minecraft:planks"],
                               "result": {"id": "minecraft:stick", "count": 4}
                           })"}}};
    const RealRecipes recipes{jar.handle()};

    // The tag is flattened as the recipe is read, nested references and all.
    CHECK(recipes.resultFor(grid(1, 1, {stack("minecraft:oak_planks")})).has_value());
    CHECK(recipes.resultFor(grid(1, 1, {stack("minecraft:birch_planks")})).has_value());
    CHECK_FALSE(recipes.resultFor(grid(1, 1, {stack("minecraft:stone")})).has_value());
}

TEST_CASE("a tag the jar does not ship leaves its recipe matching nothing") {
    FakeJar jar{.files = {{recipe("stick"), R"({
        "type": "minecraft:crafting_shapeless",
        "ingredients": ["#minecraft:planks"],
        "result": {"id": "minecraft:stick", "count": 4}
    })"}}};
    const RealRecipes recipes{jar.handle()};

    CHECK(recipes.size() == 1);
    CHECK_FALSE(recipes.resultFor(grid(1, 1, {stack("minecraft:oak_planks")})).has_value());
}

TEST_CASE("a broken tag file is skipped without costing the rest of them") {
    FakeJar jar{.files = {{itemTag("planks"), R"({"values": ["minecraft:oak_planks"]})"},
                          {itemTag("broken"), "{not json"},
                          {recipe("stick"), R"({
                               "type": "minecraft:crafting_shapeless",
                               "ingredients": ["#minecraft:planks"],
                               "result": {"id": "minecraft:stick", "count": 4}
                           })"}}};
    const RealRecipes recipes{jar.handle()};

    CHECK(recipes.resultFor(grid(1, 1, {stack("minecraft:oak_planks")})).has_value());
}

TEST_CASE("the startup line says how many were read and how many were not") {
    std::vector<std::string> lines;
    basalt::wire::common::logOutput::addSink([&lines](std::string_view line) { lines.emplace_back(line); });
    FakeJar jar{.files = {{recipe("torch"), std::string{kShapeless}}, {recipe("broken"), "{not json"}}};

    const RealRecipes recipes{jar.handle()};

    CHECK(recipes.size() == 1);
    const bool said = std::ranges::any_of(lines, [](const std::string& line) {
        return line.find("Loaded 1 crafting recipe(s) (1 other recipe type(s) skipped).") != std::string::npos;
    });
    CHECK(said);
    basalt::wire::common::logOutput::clearSinks();
}

TEST_CASE("no jar at all is an empty book rather than a crash") {
    const RealRecipes recipes{RealRecipes::Jar{}};

    CHECK(recipes.size() == 0);
    CHECK(recipes.ids().empty());
    CHECK(recipes.stonecutting().empty());
    CHECK(recipes.smithing().empty());
    CHECK(recipes.recipeFor(Identifier::vanilla("torch")) == nullptr);
    CHECK(recipes.cookingFor(crafting::Cooker::Smelting, stack("minecraft:iron_ore")) == nullptr);
    CHECK_FALSE(recipes.resultFor(crafting::CraftingInput::empty()).has_value());

    // Half a jar is no jar either, whichever half is missing.
    FakeJar jar{.files = {{recipe("torch"), std::string{kShapeless}}}};
    const RealRecipes::Jar whole = jar.handle();
    CHECK(RealRecipes{RealRecipes::Jar{.readText = whole.readText}}.size() == 0);
    CHECK(RealRecipes{RealRecipes::Jar{.entriesUnder = whole.entriesUnder}}.size() == 0);
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/advancement/AdvancementJson.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::world::advancement;
namespace text = basalt::wire::text;

namespace {

[[nodiscard]] const std::map<std::string, std::string, std::less<>>& files() {
    static const std::map<std::string, std::string, std::less<>> built{
        // No requirements at all, so they are built one group per criterion, in file order.
        {"root", R"({"display":{"title":{"translate":"advancements.story.root.title"},
                    "description":"Sleep tonight","frame":"task","announce_to_chat":"false"},
                   "criteria":{"crafting_table":{"trigger":"minecraft:inventory_changed",
                     "conditions":{"items":[{"items":"minecraft:crafting_table"}]}},
                    "planks":{"trigger":"minecraft:inventory_changed",
                     "conditions":{"items":[{"items":"#minecraft:planks"},
                                            {"items":["minecraft:stick","minecraft:flint"]}]}}}})"},
        // An explicit OR: either of the two grants it.
        {"either", R"({"parent":"minecraft:story/root",
                   "criteria":{"stone":{"trigger":"minecraft:recipe_unlocked",
                     "conditions":{"recipe":"minecraft:stone_bricks"}},
                    "brick":{"trigger":"minecraft:recipe_unlocked",
                     "conditions":{"recipe":"minecraft:bricks"}}},
                   "requirements":[["stone","brick"]],
                   "rewards":{"experience":40,"loot":["minecraft:chests/end_city_treasure"],
                              "recipes":["minecraft:elytra"]}})"},
        // An AND of two ORs, which is what the big collection advancements look like.
        {"grouped", R"({"criteria":{"a":{"trigger":"minecraft:tick"},"b":{"trigger":"minecraft:tick"},
                    "c":{"trigger":"minecraft:tick"},"d":{"trigger":"minecraft:tick"}},
                   "requirements":[["a","b"],["c","d"]]})"},
        // An empty requirements array, which is not the same as leaving it out: no group means
        // nothing to satisfy, and vanilla answers that with never complete rather than always.
        {"nothing", R"({"criteria":{"a":{"trigger":"minecraft:tick"}},"requirements":[]})"},
        // Requirements naming nothing: no group can be satisfied, so it is never complete.
        {"impossible", R"({"criteria":{"a":{"trigger":"minecraft:impossible"}},"requirements":[[]]})"},
        // A trigger with no event behind it, and two that carry no conditions block at all.
        {"unsupported", R"({"criteria":{"bred":{"trigger":"minecraft:bred_animals",
                     "conditions":{"parent":{"type":"minecraft:cow"}}},
                    "bare":{"trigger":"minecraft:inventory_changed"},
                    "bareRecipe":{"trigger":"minecraft:recipe_unlocked","conditions":{}}},
                   "requirements":[["bred"],["bare"],["bareRecipe"]]})"},
        // A display whose frame and announcement are left out, so both take their defaults.
        {"plain", R"({"display":{"title":"Adventuring Time","description":{"text":"Discover every biome"}},
                   "criteria":{"a":{"trigger":"minecraft:tick"}},"rewards":{"experience":500}})"},
        // A display with no title, which vanilla treats as no display rather than a broken one.
        {"titleless", R"({"display":{"description":"Nothing to show"},
                   "criteria":{"a":{"trigger":"minecraft:tick"}}})"},
    };
    return built;
}

[[nodiscard]] std::int64_t checksumOf(std::string_view text) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const char character : text) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

[[nodiscard]] std::string join(const std::vector<basalt::wire::common::Identifier>& items, char separator) {
    std::string text;
    for (const basalt::wire::common::Identifier& item : items) {
        if (!text.empty()) {
            text += separator;
        }
        text += item.toString();
    }
    return text;
}

[[nodiscard]] std::string describe(const AdvancementCriterion& criterion) {
    switch (criterion.condition.kind) {
    case ConditionKind::InventoryItems: {
        std::string text = "items[";
        for (std::size_t index = 0; index < criterion.condition.matchers.size(); ++index) {
            if (index != 0) {
                text += ';';
            }
            const ItemMatcher& matcher = criterion.condition.matchers[index];
            text += join(matcher.items, ',') + "|" + (matcher.tag.has_value() ? matcher.tag->toString() : "-");
        }
        return text + "]";
    }
    case ConditionKind::RecipeUnlocked:
        return "recipe[" + criterion.condition.recipe->toString() + "]";
    case ConditionKind::Unsupported:
    default:
        return "unsupported[" + criterion.trigger.toString() + "]";
    }
}

void check(std::string_view name, const std::vector<std::set<std::string, std::less<>>>& granted,
           std::string_view parent, std::string_view criteria, std::string_view groups, std::string_view rewards,
           std::string_view display, std::string_view complete, std::int64_t checksum) {
    const std::expected<Advancement, AdvancementParseError> advancement =
        advancementJson::parse(files().find(name)->second);
    REQUIRE(advancement.has_value());

    std::string criteriaText;
    for (const std::pair<std::string, AdvancementCriterion>& entry : advancement->criteria) {
        if (!criteriaText.empty()) {
            criteriaText += ';';
        }
        criteriaText += entry.first + "=" + entry.second.trigger.toString() + "/" + describe(entry.second);
    }
    std::string groupsText;
    for (std::size_t index = 0; index < advancement->requirements.groups.size(); ++index) {
        if (index != 0) {
            groupsText += ';';
        }
        for (std::size_t member = 0; member < advancement->requirements.groups[index].size(); ++member) {
            if (member != 0) {
                groupsText += ',';
            }
            groupsText += advancement->requirements.groups[index][member];
        }
    }
    const std::string rewardsText = std::to_string(advancement->rewards.experience) + "/" +
                                    join(advancement->rewards.loot, ',') + "/" +
                                    join(advancement->rewards.recipes, ',');
    std::string displayText = "-";
    if (advancement->display.has_value()) {
        displayText = text::toPlainText(advancement->display->title) + "/" +
                      text::toPlainText(advancement->display->description) + "/" + advancement->display->type + "/" +
                      (advancement->display->announceToChat ? "true" : "false");
    }
    std::string completeText;
    for (const std::set<std::string, std::less<>>& set : granted) {
        if (!completeText.empty()) {
            completeText += ',';
        }
        completeText += advancement->isComplete(set) ? "true" : "false";
    }

    CHECK((advancement->parent.has_value() ? advancement->parent->toString() : "null") == parent);
    CHECK(criteriaText == criteria);
    CHECK(groupsText == groups);
    CHECK(rewardsText == rewards);
    CHECK(displayText == display);
    CHECK(completeText == complete);
    CHECK(checksumOf(criteriaText + groupsText + rewardsText + displayText + completeText) == checksum);
}

} // namespace

TEST_CASE("the advancements parse and complete the way the frozen kotlin does") {
    SUBCASE("root") {
        check("root", {{}, {"planks"}, {"crafting_table", "planks"}}, "null",
              "crafting_table=minecraft:inventory_changed/items[minecraft:crafting_table|-];"
              "planks=minecraft:inventory_changed/items[|minecraft:planks;minecraft:stick,minecraft:flint|-]",
              "crafting_table;planks", "0//", "advancements.story.root.title/Sleep tonight/task/false",
              "false,false,true", -4423907909962067224LL);
    }
    SUBCASE("either") {
        check("either", {{}, {"stone"}, {"stone", "brick"}}, "minecraft:story/root",
              "stone=minecraft:recipe_unlocked/recipe[minecraft:stone_bricks];"
              "brick=minecraft:recipe_unlocked/recipe[minecraft:bricks]",
              "stone,brick", "40/minecraft:chests/end_city_treasure/minecraft:elytra", "-", "false,true,true",
              8648643961465794907LL);
    }
    SUBCASE("grouped") {
        check("grouped", {{}, {"a"}, {"a", "c"}, {"b", "d"}, {"a", "b"}, {"a", "b", "c"}}, "null",
              "a=minecraft:tick/unsupported[minecraft:tick];b=minecraft:tick/unsupported[minecraft:tick];"
              "c=minecraft:tick/unsupported[minecraft:tick];d=minecraft:tick/unsupported[minecraft:tick]",
              "a,b;c,d", "0//", "-", "false,false,true,true,false,true", -5877473214014099289LL);
    }
    SUBCASE("nothing") {
        check("nothing", {{}, {"a"}}, "null", "a=minecraft:tick/unsupported[minecraft:tick]", "", "0//", "-",
              "false,false", -5257698914618512666LL);
    }
    SUBCASE("impossible") {
        check("impossible", {{}, {"a"}}, "null", "a=minecraft:impossible/unsupported[minecraft:impossible]", "", "0//",
              "-", "false,false", -6905853903918045202LL);
    }
    SUBCASE("unsupported") {
        check("unsupported", {{}, {"bred", "bare", "bareRecipe"}}, "null",
              "bred=minecraft:bred_animals/unsupported[minecraft:bred_animals];"
              "bare=minecraft:inventory_changed/unsupported[minecraft:inventory_changed];"
              "bareRecipe=minecraft:recipe_unlocked/unsupported[minecraft:recipe_unlocked]",
              "bred;bare;bareRecipe", "0//", "-", "false,true", 8355912491234001644LL);
    }
    SUBCASE("plain") {
        check("plain", {{}, {"a"}}, "null", "a=minecraft:tick/unsupported[minecraft:tick]", "a", "500//",
              "Adventuring Time/Discover every biome/task/true", "false,true", 5799486277638521112LL);
    }
    SUBCASE("titleless") {
        check("titleless", {{"a"}}, "null", "a=minecraft:tick/unsupported[minecraft:tick]", "a", "0//", "-", "true",
              -5120929417135627313LL);
    }
}

TEST_CASE("the parser refuses what the frozen kotlin refuses") {
    CHECK_FALSE(advancementJson::parse(R"({"display":{"title":"x"}})").has_value());
    CHECK_FALSE(advancementJson::parse(R"({"criteria":{"a":{}}})").has_value());
}

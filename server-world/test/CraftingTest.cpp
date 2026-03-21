#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/crafting/RecipeJson.hpp>
#include <basalt/server/world/crafting/SpecialCrafting.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::world::crafting;
namespace loot = basalt::server::world::loot;
namespace nbt = basalt::wire::nbt;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] Identifier id(std::string_view name) { return *Identifier::parse(name); }

[[nodiscard]] const std::map<std::string, std::set<std::string, std::less<>>, std::less<>>& tags() {
    static const std::map<std::string, std::set<std::string, std::less<>>, std::less<>> built{
        {"minecraft:planks", {"minecraft:oak_planks", "minecraft:spruce_planks"}},
        {"minecraft:logs", {"minecraft:oak_log", "minecraft:spruce_log"}},
        {"minecraft:banners", {"minecraft:white_banner", "minecraft:red_banner"}},
    };
    return built;
}

[[nodiscard]] std::set<std::string, std::less<>> resolveTag(std::string_view tag) {
    const auto found = tags().find(tag);
    return found == tags().end() ? std::set<std::string, std::less<>>{} : found->second;
}

[[nodiscard]] const std::map<std::string, std::string, std::less<>>& recipes() {
    static const std::map<std::string, std::string, std::less<>> built{
        // A blank column and a blank row the shrink has to trim, a tag and a two-item array.
        {"pickaxe", R"({"type":"minecraft:crafting_shaped","group":"pickaxe","pattern":["### ","ABA "," X  "],
                   "key":{"#":"#minecraft:planks","A":"minecraft:stick","B":"minecraft:stick",
                          "X":["minecraft:flint","minecraft:coal"]},
                   "result":{"id":"minecraft:wooden_pickaxe","count":1}})"},
        {"torch", R"({"type":"minecraft:crafting_shaped","pattern":["C","S"],
                   "key":{"C":"minecraft:coal","S":"minecraft:stick"},
                   "result":{"id":"minecraft:torch","count":4}})"},
        // Not symmetrical, so it is also tried mirrored.
        {"banner", R"({"type":"minecraft:crafting_shaped","pattern":["WW ","W  "],
                   "key":{"W":"minecraft:white_wool"},
                   "result":{"id":"minecraft:white_banner","count":1}})"},
        {"shapeless", R"({"type":"minecraft:crafting_shapeless","group":"planks",
                   "ingredients":["#minecraft:logs","minecraft:stick","minecraft:stick"],
                   "result":{"id":"minecraft:oak_planks","count":4}})"},
        {"transmute", R"({"type":"minecraft:crafting_transmute","input":"minecraft:shulker_box",
                   "material":"minecraft:red_dye","result":{"id":"minecraft:red_shulker_box","count":1}})"},
        {"smelting", R"({"type":"minecraft:smelting","ingredient":"minecraft:iron_ore","experience":0.7,
                   "cookingtime":180,"result":{"id":"minecraft:iron_ingot","count":1}})"},
        {"blasting", R"({"type":"minecraft:blasting","ingredient":"minecraft:iron_ore","cookingtime":100,
                   "result":{"id":"minecraft:iron_ingot","count":1}})"},
        {"smoking", R"({"type":"minecraft:smoking","ingredient":"minecraft:beef","experience":0.35,
                   "cookingtime":100,"result":{"id":"minecraft:cooked_beef","count":1}})"},
        {"campfire", R"({"type":"minecraft:campfire_cooking","ingredient":"minecraft:beef","cookingtime":100,
                   "result":{"id":"minecraft:cooked_beef","count":1}})"},
        // A blank first row, which only the shrink's own leading-row counter removes.
        {"leading", R"({"type":"minecraft:crafting_shaped","pattern":["  ","CC","S "],
                   "key":{"C":"minecraft:coal","S":"minecraft:stick"},
                   "result":{"id":"minecraft:torch","count":4}})"},
        {"smeltingDefault", R"({"type":"minecraft:smelting","ingredient":"minecraft:iron_ore","cookingtime":200,
                   "result":{"id":"minecraft:iron_ingot","count":1}})"},
        {"stonecutting", R"({"type":"minecraft:stonecutting","ingredient":"minecraft:stone",
                   "result":{"id":"minecraft:stone_brick_slab","count":2}})"},
        {"transform", R"({"type":"minecraft:smithing_transform","template":"minecraft:netherite_upgrade",
                   "base":"minecraft:diamond_sword","addition":"minecraft:netherite_ingot",
                   "result":{"id":"minecraft:netherite_sword","count":1}})"},
        // No result of its own: what comes out is the base with the pattern written on it.
        {"trim", R"({"type":"minecraft:smithing_trim","template":"minecraft:coast_armor_trim_smithing_template",
                   "base":"minecraft:iron_chestplate","addition":["minecraft:iron_ingot","minecraft:gold_ingot"],
                   "pattern":"minecraft:coast"})"},
        {"dye", R"({"type":"minecraft:crafting_dye","target":"minecraft:leather_helmet",
                   "dye":["minecraft:red_dye","minecraft:blue_dye","minecraft:yellow_dye"],
                   "result":{"id":"minecraft:leather_helmet","count":1}})"},
        {"imbue", R"({"type":"minecraft:crafting_imbue","source":"minecraft:lingering_potion",
                   "material":"minecraft:arrow","result":{"id":"minecraft:tipped_arrow","count":8}})"},
        {"pot", R"({"type":"minecraft:crafting_decorated_pot","back":"minecraft:brick",
                   "result":{"id":"minecraft:decorated_pot"}})"},
        {"bookcloning", R"({"type":"minecraft:crafting_special_bookcloning","source":"minecraft:written_book",
                   "material":"minecraft:writable_book","result":{"id":"minecraft:written_book","count":1}})"},
        {"bannerduplicate", R"({"type":"minecraft:crafting_special_bannerduplicate","banner":"#minecraft:banners",
                   "result":{"id":"minecraft:white_banner","count":1}})"},
        {"shielddecoration", R"({"type":"minecraft:crafting_special_shielddecoration","target":"minecraft:shield",
                   "banner":"#minecraft:banners","result":{"id":"minecraft:shield","count":1}})"},
        {"mapextending", R"({"type":"minecraft:crafting_special_mapextending","map":"minecraft:filled_map",
                   "material":"minecraft:paper","result":{"id":"minecraft:filled_map","count":1}})"},
        {"rocket", R"({"type":"minecraft:crafting_special_firework_rocket","shell":"minecraft:paper",
                   "fuel":"minecraft:gunpowder","star":"minecraft:firework_star",
                   "result":{"id":"minecraft:firework_rocket","count":3}})"},
        {"star", R"({"type":"minecraft:crafting_special_firework_star","fuel":"minecraft:gunpowder",
                   "dye":["minecraft:red_dye","minecraft:blue_dye","minecraft:yellow_dye"],
                   "shapes":{"burst":"minecraft:feather","creeper":"minecraft:creeper_head",
                             "large_ball":"minecraft:fire_charge","star":"minecraft:gold_nugget"},
                   "trail":"minecraft:diamond","twinkle":"minecraft:glowstone_dust",
                   "result":{"id":"minecraft:firework_star","count":1}})"},
        {"starfade", R"({"type":"minecraft:crafting_special_firework_star_fade","target":"minecraft:firework_star",
                   "dye":["minecraft:red_dye","minecraft:blue_dye"],
                   "result":{"id":"minecraft:firework_star","count":1}})"},
        {"repair", R"({"type":"minecraft:crafting_special_repairitem",
                   "result":{"id":"minecraft:air","count":1}})"},
    };
    return built;
}

[[nodiscard]] loot::ItemStack stack(std::string_view name, std::int32_t count = 1,
                                    nbt::NbtCompound components = nbt::NbtCompound{}) {
    return loot::ItemStack{.item = id(name), .count = count, .components = std::move(components)};
}

[[nodiscard]] nbt::NbtCompound withInt(std::string_view key, std::int32_t value) {
    nbt::NbtCompound components;
    components.set(key, nbt::NbtTag::ofInt(value));
    return components;
}

[[nodiscard]] nbt::NbtCompound patterned() {
    nbt::NbtCompound pattern;
    pattern.set("pattern", nbt::NbtTag::ofString("minecraft:stripe_bottom"));
    pattern.set("color", nbt::NbtTag::ofString("red"));
    nbt::NbtList list{nbt::NbtType::Compound};
    list.push(nbt::NbtTag::ofCompound(std::move(pattern)));
    nbt::NbtCompound components;
    components.set(specialRecipes::kBannerPatterns, nbt::NbtTag::ofList(std::move(list)));
    return components;
}

struct Grid {
    std::int32_t width;
    std::int32_t height;
    std::vector<std::optional<loot::ItemStack>> items;
};

[[nodiscard]] const std::map<std::string, Grid, std::less<>>& grids() {
    static const std::map<std::string, Grid, std::less<>> built{
        // The pickaxe as authored, in the bottom-left of a 3x3 grid.
        {"pickaxe", Grid{3,
                         3,
                         {stack("minecraft:oak_planks"), stack("minecraft:spruce_planks"),
                          stack("minecraft:oak_planks"), stack("minecraft:stick"), stack("minecraft:stick"),
                          stack("minecraft:stick"), std::nullopt, stack("minecraft:flint"), std::nullopt}}},
        // The same shape one column right, which the trim makes the same input.
        {"pickaxeShifted", Grid{3,
                                3,
                                {stack("minecraft:oak_planks"), stack("minecraft:oak_planks"),
                                 stack("minecraft:oak_planks"), stack("minecraft:stick"), stack("minecraft:stick"),
                                 stack("minecraft:stick"), stack("minecraft:coal"), std::nullopt, std::nullopt}}},
        {"torch", Grid{1, 2, {stack("minecraft:coal"), stack("minecraft:stick")}}},
        {"torchWrong", Grid{1, 2, {stack("minecraft:stick"), stack("minecraft:coal")}}},
        // The same two, inset, so the trim has to move both the column and the row.
        {"torchInset", Grid{3,
                            3,
                            {std::nullopt, std::nullopt, std::nullopt, std::nullopt, stack("minecraft:coal"),
                             std::nullopt, std::nullopt, stack("minecraft:stick"), std::nullopt}}},
        {"leading",
         Grid{2, 2, {stack("minecraft:coal"), stack("minecraft:coal"), stack("minecraft:stick"), std::nullopt}}},
        // The banner shape mirrored: only the flip pass accepts it.
        {"bannerMirror", Grid{2,
                              2,
                              {stack("minecraft:white_wool"), stack("minecraft:white_wool"), std::nullopt,
                               stack("minecraft:white_wool")}}},
        // The banner shape as authored, which only the unflipped pass accepts.
        {"bannerPlain", Grid{2,
                             2,
                             {stack("minecraft:white_wool"), stack("minecraft:white_wool"),
                              stack("minecraft:white_wool"), std::nullopt}}},
        {"shapeless",
         Grid{2, 2, {stack("minecraft:stick"), stack("minecraft:spruce_log"), stack("minecraft:stick"), std::nullopt}}},
        // Three sticks and no log: the matching has to fail, not just count to three.
        {"shapelessWrong",
         Grid{2, 2, {stack("minecraft:stick"), stack("minecraft:stick"), stack("minecraft:stick"), std::nullopt}}},
        {"transmute", Grid{2, 1, {stack("minecraft:red_dye"), stack("minecraft:shulker_box")}}},
        {"one", Grid{1, 1, {stack("minecraft:iron_ore")}}},
        {"oneBeef", Grid{1, 1, {stack("minecraft:beef")}}},
        {"oneStone", Grid{1, 1, {stack("minecraft:stone")}}},
        // A bucket, whose emptied form is what the grid is left holding.
        {"bucketed", Grid{2, 1, {stack("minecraft:water_bucket"), stack("minecraft:oak_log")}}},
        {"dye", Grid{2,
                     2,
                     {stack("minecraft:leather_helmet"), stack("minecraft:red_dye"), stack("minecraft:blue_dye"),
                      std::nullopt}}},
        // Already dyed, so the mix averages against what it had.
        {"dyeAgain", Grid{2,
                          1,
                          {stack("minecraft:leather_helmet", 1, withInt(specialRecipes::kDyedColor, 0x102030)),
                           stack("minecraft:yellow_dye")}}},
        // A dye with no colour behind it, which makes the whole thing not a recipe.
        {"dyeUnknown", Grid{2, 1, {stack("minecraft:leather_helmet"), stack("minecraft:green_dye")}}},
        {"imbue",
         Grid{3,
              3,
              {stack("minecraft:arrow"), stack("minecraft:arrow"), stack("minecraft:arrow"), stack("minecraft:arrow"),
               stack("minecraft:lingering_potion", 1,
                     [] {
                         nbt::NbtCompound components;
                         components.set("minecraft:potion_contents", nbt::NbtTag::ofString("minecraft:strength"));
                         return components;
                     }()),
               stack("minecraft:arrow"), stack("minecraft:arrow"), stack("minecraft:arrow"),
               stack("minecraft:arrow")}}},
        {"pot", Grid{3,
                     3,
                     {std::nullopt, stack("minecraft:brick"), std::nullopt, stack("minecraft:brick"), std::nullopt,
                      stack("minecraft:brick"), std::nullopt, stack("minecraft:brick"), std::nullopt}}},
        {"map",
         Grid{3,
              3,
              {stack("minecraft:paper"), stack("minecraft:paper"), stack("minecraft:paper"), stack("minecraft:paper"),
               stack("minecraft:filled_map", 1, withInt("minecraft:map_id", 7)), stack("minecraft:paper"),
               stack("minecraft:paper"), stack("minecraft:paper"), stack("minecraft:paper")}}},
        {"bookcloning", Grid{2,
                             2,
                             {stack("minecraft:writable_book", 2),
                              stack("minecraft:written_book", 1,
                                    [] {
                                        nbt::NbtCompound components;
                                        components.set("minecraft:written_book_content",
                                                       nbt::NbtTag::ofString("a title"));
                                        return components;
                                    }()),
                              stack("minecraft:writable_book", 1), std::nullopt}}},
        {"bannerduplicate",
         Grid{2, 1, {stack("minecraft:white_banner"), stack("minecraft:red_banner", 1, patterned())}}},
        // Two blanks: nothing to copy from, so no recipe.
        {"bannerBlanks", Grid{2, 1, {stack("minecraft:white_banner"), stack("minecraft:red_banner")}}},
        {"shield", Grid{2, 1, {stack("minecraft:shield"), stack("minecraft:red_banner", 1, patterned())}}},
        // A shield that already carries patterns is not a recipe.
        {"shieldUsed",
         Grid{2, 1, {stack("minecraft:shield", 1, patterned()), stack("minecraft:red_banner", 1, patterned())}}},
        {"rocket", Grid{2,
                        2,
                        {stack("minecraft:paper"), stack("minecraft:gunpowder"), stack("minecraft:gunpowder"),
                         stack("minecraft:firework_star")}}},
        // Four gunpowder is past the flight cap, which is one to three.
        {"rocketOverfuelled",
         Grid{3,
              2,
              {stack("minecraft:paper"), stack("minecraft:gunpowder"), stack("minecraft:gunpowder"),
               stack("minecraft:gunpowder"), stack("minecraft:gunpowder"), std::nullopt}}},
        {"star", Grid{2,
                      2,
                      {stack("minecraft:gunpowder"), stack("minecraft:red_dye"), stack("minecraft:blue_dye"),
                       stack("minecraft:diamond")}}},
        {"starShaped",
         Grid{3,
              2,
              {stack("minecraft:gunpowder"), stack("minecraft:yellow_dye"), stack("minecraft:gold_nugget"),
               stack("minecraft:glowstone_dust"), std::nullopt, std::nullopt}}},
        // A star with no dye at all, which is not a recipe however much else is right.
        {"starNoDye", Grid{2, 1, {stack("minecraft:gunpowder"), stack("minecraft:diamond")}}},
        // Two shapes at once, which is not a recipe however many dyes there are.
        {"starTwoShapes",
         Grid{3,
              2,
              {stack("minecraft:gunpowder"), stack("minecraft:red_dye"), stack("minecraft:gold_nugget"),
               stack("minecraft:feather"), std::nullopt, std::nullopt}}},
        {"starfade", Grid{2,
                          1,
                          {stack("minecraft:firework_star", 1, withInt(specialRecipes::kDyedColor, 0xB02E26)),
                           stack("minecraft:blue_dye")}}},
        {"repair", Grid{2,
                        1,
                        {stack("minecraft:iron_pickaxe", 1, withInt(specialRecipes::kDamage, 200)),
                         stack("minecraft:iron_pickaxe", 1, withInt(specialRecipes::kDamage, 30))}}},
        // Both worn far enough that the repair lands short of full, which is where the bonus shows.
        {"repairWorn", Grid{2,
                            1,
                            {stack("minecraft:iron_pickaxe", 1, withInt(specialRecipes::kDamage, 200)),
                             stack("minecraft:iron_pickaxe", 1, withInt(specialRecipes::kDamage, 200))}}},
        // Two of a kind that takes no damage at all is not a repair.
        {"repairSticks", Grid{2, 1, {stack("minecraft:stick"), stack("minecraft:stick")}}},
        // A stack of two never repairs, however damaged it is.
        {"repairStacked", Grid{2,
                               1,
                               {stack("minecraft:iron_pickaxe", 2, withInt(specialRecipes::kDamage, 200)),
                                stack("minecraft:iron_pickaxe", 1)}}},
        {"empty", Grid{2, 2, {std::nullopt, std::nullopt, std::nullopt, std::nullopt}}},
        // One empty slot, whose bounding box is the one that does not collapse.
        {"emptyOne", Grid{1, 1, {std::nullopt}}},
    };
    return built;
}

void installHooks() {
    hooks::setRemainderOf([](const Identifier& item) -> std::optional<Identifier> {
        const std::string name = item.toString();
        if (name == "minecraft:water_bucket" || name == "minecraft:lava_bucket") {
            return id("minecraft:bucket");
        }
        if (name == "minecraft:honey_bottle") {
            return id("minecraft:glass_bottle");
        }
        return std::nullopt;
    });
    hooks::setMaxDamageOf([](const Identifier& item) -> std::int32_t {
        const std::string name = item.toString();
        if (name == "minecraft:iron_pickaxe") {
            return 250;
        }
        if (name == "minecraft:diamond_sword") {
            return 1561;
        }
        return 0;
    });
    hooks::setDyeColourOf([](const Identifier& item) -> std::optional<std::int32_t> {
        const std::string name = item.toString();
        if (name == "minecraft:red_dye") {
            return 0xB02E26;
        }
        if (name == "minecraft:blue_dye") {
            return 0x3C44AA;
        }
        if (name == "minecraft:yellow_dye") {
            return 0xFED83D;
        }
        if (name == "minecraft:white_dye") {
            return 0xF9FFFE;
        }
        return std::nullopt;
    });
    hooks::setTrimMaterialOf([](const Identifier& item) -> std::optional<Identifier> {
        const std::string name = item.toString();
        if (name == "minecraft:iron_ingot") {
            return id("minecraft:iron");
        }
        if (name == "minecraft:gold_ingot") {
            return id("minecraft:gold");
        }
        return std::nullopt;
    });
}

[[nodiscard]] std::int64_t checksumOf(std::string_view text) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const char character : text) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int64_t>(checksum);
}

[[nodiscard]] std::string describe(const std::optional<loot::ItemStack>& item) {
    if (!item.has_value()) {
        return "-";
    }
    return item->item.toString() + "x" + std::to_string(item->count) +
           nbt::snbt::stringify(nbt::NbtTag::ofCompound(item->components));
}

/// The oracle prints a Kotlin Float, which always carries a decimal point.
[[nodiscard]] std::string floatOf(float value) {
    std::string text = std::format("{}", value);
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos) {
        text += ".0";
    }
    return text;
}

[[nodiscard]] std::string join(const std::set<std::string, std::less<>>& items) {
    std::string text;
    for (const std::string& item : items) {
        if (!text.empty()) {
            text += ',';
        }
        text += item;
    }
    return text;
}

[[nodiscard]] std::string join(const std::optional<Ingredient>& ingredient) {
    return ingredient.has_value() ? join(ingredient->items) : "-";
}

/// The parsed recipe itself, so the schema is checked apart from what the matcher does with it.
[[nodiscard]] std::string shapeOf(const Recipe& recipe) {
    switch (recipe.kind) {
    case RecipeKind::Shaped: {
        std::string text = "shaped/" + std::to_string(recipe.width) + "x" + std::to_string(recipe.height) + "/";
        for (std::size_t index = 0; index < recipe.shape.size(); ++index) {
            if (index != 0) {
                text += ';';
            }
            text += join(recipe.shape[index]);
        }
        return text;
    }
    case RecipeKind::Shapeless: {
        std::string text = "shapeless/";
        for (std::size_t index = 0; index < recipe.ingredients.size(); ++index) {
            if (index != 0) {
                text += ';';
            }
            text += join(recipe.ingredients[index].items);
        }
        return text;
    }
    case RecipeKind::Cooking: {
        static constexpr std::string_view kCookers[] = {"SMELTING", "BLASTING", "SMOKING", "CAMPFIRE"};
        return std::string{"cooking/"} + std::string{kCookers[static_cast<std::size_t>(recipe.cooker)]} + "/" +
               floatOf(recipe.experience) + "/" + std::to_string(recipe.cookingTime) + "/" +
               join(recipe.ingredient.items);
    }
    case RecipeKind::Stonecutting:
        return "stonecutting/" + join(recipe.ingredient.items);
    case RecipeKind::Smithing:
        return "smithing/" + (recipe.trimPattern.has_value() ? recipe.trimPattern->toString() : "null") + "/" +
               join(recipe.smithingTemplate.items) + ";" + join(recipe.base.items) + ";" + join(recipe.addition.items);
    case RecipeKind::Special:
    default: {
        static constexpr std::string_view kKinds[] = {"DYE",
                                                      "IMBUE",
                                                      "BOOK_CLONING",
                                                      "BANNER_DUPLICATE",
                                                      "FIREWORK_ROCKET",
                                                      "FIREWORK_STAR",
                                                      "FIREWORK_STAR_FADE",
                                                      "MAP_EXTENDING",
                                                      "SHIELD_DECORATION",
                                                      "REPAIR_ITEM",
                                                      "DECORATED_POT"};
        std::string text = std::string{"special/"} + std::string{kKinds[static_cast<std::size_t>(recipe.specialKind)]} +
                           "/" + join(recipe.target) + ";" + join(recipe.material) + ";" + join(recipe.extra) + "/";
        for (std::size_t index = 0; index < recipe.modifiers.size(); ++index) {
            if (index != 0) {
                text += ';';
            }
            text += recipe.modifiers[index].first + "=" + join(recipe.modifiers[index].second.items);
        }
        return text;
    }
    }
}

void check(std::string_view recipeName, std::string_view gridName, bool matches, std::string_view group,
           std::string_view shape, std::string_view result, std::string_view remaining, std::int64_t checksum) {
    const std::expected<Recipe, RecipeParseError> recipe =
        recipeJson::parse(recipes().find(recipeName)->second, resolveTag);
    REQUIRE(recipe.has_value());
    const Grid& grid = grids().find(gridName)->second;
    const CraftingInput input = CraftingInput::of(grid.width, grid.height, grid.items);
    const std::string assembled = describe(recipe->assemble(input));
    std::string left;
    for (const std::optional<loot::ItemStack>& item : recipe->remainingItems(input)) {
        if (!left.empty()) {
            left += ',';
        }
        left += describe(item);
    }
    CHECK(recipe->matches(input) == matches);
    CHECK(recipe->group == group);
    CHECK(shapeOf(*recipe) == shape);
    CHECK(assembled == result);
    CHECK(left == remaining);
    CHECK(checksumOf(std::string{matches ? "true" : "false"} + assembled + left) == checksum);
}

/// The two questions a smithing table asks, which no crafting grid ever reaches.
void checkSmithing(std::string_view recipeName, const loot::ItemStack& smithingTemplate, const loot::ItemStack& base,
                   const loot::ItemStack& addition, bool accepts, std::string_view shape, std::string_view result) {
    const std::expected<Recipe, RecipeParseError> recipe =
        recipeJson::parse(recipes().find(recipeName)->second, resolveTag);
    REQUIRE(recipe.has_value());
    CHECK(recipe->acceptsSmithing(&smithingTemplate, &base, &addition) == accepts);
    CHECK(shapeOf(*recipe) == shape);
    CHECK(describe(recipe->assembleSmithing(base, addition)) == result);
}

[[nodiscard]] std::string refusal(std::string_view json) {
    const std::expected<Recipe, RecipeParseError> recipe = recipeJson::parse(json, resolveTag);
    if (recipe.has_value()) {
        return "accepted";
    }
    std::string message = recipe.error().message;
    for (char& character : message) {
        if (character == ' ') {
            character = '_';
        }
    }
    return message;
}

/// The chestplate that already wears the trim the recipe would apply.
[[nodiscard]] nbt::NbtCompound alreadyTrimmed() {
    nbt::NbtCompound trim;
    trim.set("pattern", nbt::NbtTag::ofString("minecraft:coast"));
    trim.set("material", nbt::NbtTag::ofString("minecraft:gold"));
    nbt::NbtCompound components;
    components.set(specialRecipes::kTrim, nbt::NbtTag::ofCompound(std::move(trim)));
    return components;
}

} // namespace

TEST_CASE("the recipes match and assemble what the frozen kotlin does") {
    installHooks();
    SUBCASE("pickaxe") {
        check("pickaxe", "pickaxe", true, "pickaxe",
              "shaped/3x3/"
              "minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_"
              "planks,minecraft:spruce_planks;minecraft:stick;minecraft:stick;minecraft:stick;-;minecraft:coal,"
              "minecraft:flint;-",
              "minecraft:wooden_pickaxex1{}", "-,-,-,-,-,-,-,-,-", 9014994880917023810LL);
    }
    SUBCASE("pickaxeShifted") {
        check("pickaxe", "pickaxeShifted", false, "pickaxe",
              "shaped/3x3/"
              "minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_"
              "planks,minecraft:spruce_planks;minecraft:stick;minecraft:stick;minecraft:stick;-;minecraft:coal,"
              "minecraft:flint;-",
              "minecraft:wooden_pickaxex1{}", "-,-,-,-,-,-,-,-,-", 5405765277663816793LL);
    }
    SUBCASE("pickaxeWrongGrid") {
        check("pickaxe", "torch", false, "pickaxe",
              "shaped/3x3/"
              "minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_planks,minecraft:spruce_planks;minecraft:oak_"
              "planks,minecraft:spruce_planks;minecraft:stick;minecraft:stick;minecraft:stick;-;minecraft:coal,"
              "minecraft:flint;-",
              "minecraft:wooden_pickaxex1{}", "-,-", -1673508879002295866LL);
    }
    SUBCASE("torch") {
        check("torch", "torch", true, "", "shaped/1x2/minecraft:coal;minecraft:stick", "minecraft:torchx4{}", "-,-",
              -1899159346839386860LL);
    }
    SUBCASE("torchWrong") {
        check("torch", "torchWrong", false, "", "shaped/1x2/minecraft:coal;minecraft:stick", "minecraft:torchx4{}",
              "-,-", -1642241552085222467LL);
    }
    SUBCASE("bannerMirror") {
        check("banner", "bannerMirror", true, "",
              "shaped/2x2/minecraft:white_wool;minecraft:white_wool;minecraft:white_wool;-",
              "minecraft:white_bannerx1{}", "-,-,-,-", 5532978426363250635LL);
    }
    SUBCASE("bannerPlain") {
        check("banner", "bannerPlain", true, "",
              "shaped/2x2/minecraft:white_wool;minecraft:white_wool;minecraft:white_wool;-",
              "minecraft:white_bannerx1{}", "-,-,-,-", 5532978426363250635LL);
    }
    SUBCASE("torchInset") {
        check("torch", "torchInset", true, "", "shaped/1x2/minecraft:coal;minecraft:stick", "minecraft:torchx4{}",
              "-,-", -1899159346839386860LL);
    }
    SUBCASE("leading") {
        check("leading", "leading", true, "", "shaped/2x2/minecraft:coal;minecraft:coal;minecraft:stick;-",
              "minecraft:torchx4{}", "-,-,-,-", -1437319875475806676LL);
    }
    SUBCASE("shapeless") {
        check("shapeless", "shapeless", true, "planks",
              "shapeless/minecraft:oak_log,minecraft:spruce_log;minecraft:stick;minecraft:stick",
              "minecraft:oak_planksx4{}", "-,-,-,-", 4153508481604496899LL);
    }
    SUBCASE("shapelessWrong") {
        check("shapeless", "shapelessWrong", false, "planks",
              "shapeless/minecraft:oak_log,minecraft:spruce_log;minecraft:stick;minecraft:stick",
              "minecraft:oak_planksx4{}", "-,-,-,-", -7493901610918741668LL);
    }
    SUBCASE("shapelessRemainder") {
        check("shapeless", "bucketed", false, "planks",
              "shapeless/minecraft:oak_log,minecraft:spruce_log;minecraft:stick;minecraft:stick",
              "minecraft:oak_planksx4{}", "minecraft:bucketx1{},-", 1484052535415358781LL);
    }
    SUBCASE("transmute") {
        check("transmute", "transmute", true, "", "shapeless/minecraft:shulker_box;minecraft:red_dye",
              "minecraft:red_shulker_boxx1{}", "-,-", -7822519764603410355LL);
    }
    SUBCASE("smelting") {
        check("smelting", "one", true, "", "cooking/SMELTING/0.7/180/minecraft:iron_ore", "minecraft:iron_ingotx1{}",
              "-", 1198202942410915542LL);
    }
    SUBCASE("blasting") {
        check("blasting", "one", true, "", "cooking/BLASTING/0.0/100/minecraft:iron_ore", "minecraft:iron_ingotx1{}",
              "-", 1198202942410915542LL);
    }
    SUBCASE("smoking") {
        check("smoking", "oneBeef", true, "", "cooking/SMOKING/0.35/100/minecraft:beef", "minecraft:cooked_beefx1{}",
              "-", 4993148480502366096LL);
    }
    SUBCASE("campfire") {
        check("campfire", "oneBeef", true, "", "cooking/CAMPFIRE/0.0/100/minecraft:beef", "minecraft:cooked_beefx1{}",
              "-", 4993148480502366096LL);
    }
    SUBCASE("smeltingWrong") {
        check("smelting", "oneBeef", false, "", "cooking/SMELTING/0.7/180/minecraft:iron_ore",
              "minecraft:iron_ingotx1{}", "-", -5421893796085531723LL);
    }
    SUBCASE("smeltingDefault") {
        check("smeltingDefault", "one", true, "", "cooking/SMELTING/0.0/200/minecraft:iron_ore",
              "minecraft:iron_ingotx1{}", "-", 1198202942410915542LL);
    }
    SUBCASE("stonecutting") {
        check("stonecutting", "oneStone", true, "", "stonecutting/minecraft:stone", "minecraft:stone_brick_slabx2{}",
              "-", -4405922648049568481LL);
    }
    SUBCASE("smithingInGrid") {
        check("transform", "one", false, "",
              "smithing/null/minecraft:netherite_upgrade;minecraft:diamond_sword;minecraft:netherite_ingot",
              "minecraft:netherite_swordx1{}", "-", 8319985270148597221LL);
    }
    SUBCASE("dye") {
        check("dye", "dye", true, "",
              "special/DYE/minecraft:leather_helmet;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/",
              R"(minecraft:leather_helmetx1{"minecraft:dyed_color":11359128})", "-,-,-,-", -3954546230669117631LL);
    }
    SUBCASE("dyeAgain") {
        check("dye", "dyeAgain", true, "",
              "special/DYE/minecraft:leather_helmet;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/",
              R"(minecraft:leather_helmetx1{"minecraft:dyed_color":9931324})", "-,-", -4014489896636574626LL);
    }
    SUBCASE("dyeUnknown") {
        check("dye", "dyeUnknown", false, "",
              "special/DYE/minecraft:leather_helmet;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/",
              "minecraft:leather_helmetx1{}", "-,-", -5153693101257134279LL);
    }
    SUBCASE("imbue") {
        check("imbue", "imbue", true, "", "special/IMBUE/minecraft:lingering_potion;minecraft:arrow;-/",
              R"(minecraft:tipped_arrowx8{"minecraft:potion_contents":"minecraft:strength"})", "-,-,-,-,-,-,-,-,-",
              -4727609943312515470LL);
    }
    SUBCASE("imbueWrong") {
        check("imbue", "map", false, "", "special/IMBUE/minecraft:lingering_potion;minecraft:arrow;-/",
              "minecraft:tipped_arrowx8{}", "-,-,-,-,-,-,-,-,-", -6866018611245368902LL);
    }
    SUBCASE("pot") {
        check("pot", "pot", true, "", "special/DECORATED_POT/-;minecraft:brick;-/", "minecraft:decorated_potx1{}",
              "-,-,-,-,-,-,-,-,-", -5253826018343243157LL);
    }
    SUBCASE("mapextending") {
        check("mapextending", "map", true, "", "special/MAP_EXTENDING/minecraft:filled_map;minecraft:paper;-/",
              R"(minecraft:filled_mapx1{"minecraft:map_id":7})", "-,-,-,-,-,-,-,-,-", -4605486320582613029LL);
    }
    SUBCASE("bookcloning") {
        check("bookcloning", "bookcloning", true, "",
              "special/BOOK_CLONING/minecraft:written_book;minecraft:writable_book;-/",
              R"(minecraft:written_bookx3{"minecraft:written_book_content":"a title"})",
              R"(-,minecraft:written_bookx1{"minecraft:written_book_content":"a title"},-,-)", 5714768518351086806LL);
    }
    SUBCASE("bannerduplicate") {
        check(
            "bannerduplicate", "bannerduplicate", true, "",
            "special/BANNER_DUPLICATE/minecraft:red_banner,minecraft:white_banner;-;-/",
            R"(minecraft:white_bannerx1{"minecraft:banner_patterns":[{pattern:"minecraft:stripe_bottom",color:"red"}]})",
            "-,-", 2932441190602937870LL);
    }
    SUBCASE("bannerBlanks") {
        check("bannerduplicate", "bannerBlanks", false, "",
              "special/BANNER_DUPLICATE/minecraft:red_banner,minecraft:white_banner;-;-/", "minecraft:white_bannerx1{}",
              "-,-", -1990050740634138476LL);
    }
    SUBCASE("shield") {
        check("shielddecoration", "shield", true, "",
              "special/SHIELD_DECORATION/minecraft:shield;minecraft:red_banner,minecraft:white_banner;-/",
              R"(minecraft:shieldx1{"minecraft:banner_patterns":[{pattern:"minecraft:stripe_bottom",color:"red"}]})",
              "-,-", -3423110269336130881LL);
    }
    SUBCASE("shieldUsed") {
        check("shielddecoration", "shieldUsed", false, "",
              "special/SHIELD_DECORATION/minecraft:shield;minecraft:red_banner,minecraft:white_banner;-/",
              "minecraft:shieldx1{}", "-,-", 3538584282945437815LL);
    }
    SUBCASE("rocket") {
        check("rocket", "rocket", true, "",
              "special/FIREWORK_ROCKET/minecraft:paper;minecraft:gunpowder;minecraft:firework_star/",
              R"(minecraft:firework_rocketx3{"minecraft:fireworks_flight_duration":2})", "-,-,-,-",
              6271653146104092066LL);
    }
    SUBCASE("rocketOverfuelled") {
        check("rocket", "rocketOverfuelled", false, "",
              "special/FIREWORK_ROCKET/minecraft:paper;minecraft:gunpowder;minecraft:firework_star/",
              "minecraft:firework_rocketx3{}", "-,-,-,-,-,-", -3141090122919794978LL);
    }
    SUBCASE("star") {
        check(
            "star", "star", true, "",
            "special/FIREWORK_STAR/minecraft:gunpowder;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/"
            "burst=minecraft:feather;creeper=minecraft:creeper_head;large_ball=minecraft:fire_charge;star=minecraft:"
            "gold_nugget;trail=minecraft:diamond;twinkle=minecraft:glowstone_dust",
            R"(minecraft:firework_starx1{"minecraft:firework_explosion_colors":[11546150,3949738],"minecraft:firework_explosion_trail":1})",
            "-,-,-,-", 7902481665997004892LL);
    }
    SUBCASE("starShaped") {
        check(
            "star", "starShaped", true, "",
            "special/FIREWORK_STAR/minecraft:gunpowder;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/"
            "burst=minecraft:feather;creeper=minecraft:creeper_head;large_ball=minecraft:fire_charge;star=minecraft:"
            "gold_nugget;trail=minecraft:diamond;twinkle=minecraft:glowstone_dust",
            R"(minecraft:firework_starx1{"minecraft:firework_explosion_colors":[16701501],"minecraft:firework_explosion_shape":"star","minecraft:firework_explosion_twinkle":1})",
            "-,-,-,-,-,-", 2061206465340163355LL);
    }
    SUBCASE("starNoDye") {
        check("star", "starNoDye", false, "",
              "special/FIREWORK_STAR/minecraft:gunpowder;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/"
              "burst=minecraft:feather;creeper=minecraft:creeper_head;large_ball=minecraft:fire_charge;star=minecraft:"
              "gold_nugget;trail=minecraft:diamond;twinkle=minecraft:glowstone_dust",
              "minecraft:firework_starx1{}", "-,-", 2257877619941062812LL);
    }
    SUBCASE("starTwoShapes") {
        check("star", "starTwoShapes", false, "",
              "special/FIREWORK_STAR/minecraft:gunpowder;minecraft:blue_dye,minecraft:red_dye,minecraft:yellow_dye;-/"
              "burst=minecraft:feather;creeper=minecraft:creeper_head;large_ball=minecraft:fire_charge;star=minecraft:"
              "gold_nugget;trail=minecraft:diamond;twinkle=minecraft:glowstone_dust",
              "minecraft:firework_starx1{}", "-,-,-,-,-,-", -2480855332155511636LL);
    }
    SUBCASE("starfade") {
        check("starfade", "starfade", true, "",
              "special/FIREWORK_STAR_FADE/minecraft:firework_star;minecraft:blue_dye,minecraft:red_dye;-/",
              R"(minecraft:firework_starx1{"minecraft:dyed_color":11359128})", "-,-", -5017026861395358188LL);
    }
    SUBCASE("repair") {
        check("repair", "repair", true, "", "special/REPAIR_ITEM/-;-;-/",
              R"(minecraft:iron_pickaxex1{"minecraft:damage":0})", "-,-", 4471627845116210501LL);
    }
    SUBCASE("repairWorn") {
        check("repair", "repairWorn", true, "", "special/REPAIR_ITEM/-;-;-/",
              R"(minecraft:iron_pickaxex1{"minecraft:damage":138})", "-,-", -629076778759763267LL);
    }
    SUBCASE("repairSticks") {
        check("repair", "repairSticks", false, "", "special/REPAIR_ITEM/-;-;-/", "minecraft:airx1{}", "-,-",
              -6012176411596613706LL);
    }
    SUBCASE("repairStacked") {
        check("repair", "repairStacked", false, "", "special/REPAIR_ITEM/-;-;-/", "minecraft:airx1{}", "-,-",
              -6012176411596613706LL);
    }
    SUBCASE("repairEmpty") {
        check("repair", "empty", false, "", "special/REPAIR_ITEM/-;-;-/", "minecraft:airx1{}", "",
              8358681019388242174LL);
    }
    SUBCASE("repairEmptyOne") {
        check("repair", "emptyOne", false, "", "special/REPAIR_ITEM/-;-;-/", "minecraft:airx1{}", "-",
              -8383170429355041143LL);
    }
}

TEST_CASE("the smithing table accepts and assembles what the frozen kotlin does") {
    installHooks();
    SUBCASE("transform") {
        checkSmithing("transform", stack("minecraft:netherite_upgrade"),
                      stack("minecraft:diamond_sword", 1, withInt(specialRecipes::kDamage, 12)),
                      stack("minecraft:netherite_ingot"), true,
                      "smithing/null/minecraft:netherite_upgrade;minecraft:diamond_sword;minecraft:netherite_ingot",
                      R"(minecraft:netherite_swordx1{"minecraft:damage":12})");
    }
    SUBCASE("transformWrongBase") {
        checkSmithing("transform", stack("minecraft:netherite_upgrade"), stack("minecraft:iron_sword"),
                      stack("minecraft:netherite_ingot"), false,
                      "smithing/null/minecraft:netherite_upgrade;minecraft:diamond_sword;minecraft:netherite_ingot",
                      "minecraft:netherite_swordx1{}");
    }
    SUBCASE("trim") {
        checkSmithing(
            "trim", stack("minecraft:coast_armor_trim_smithing_template"), stack("minecraft:iron_chestplate"),
            stack("minecraft:gold_ingot"), true,
            "smithing/minecraft:coast/"
            "minecraft:coast_armor_trim_smithing_template;minecraft:iron_chestplate;minecraft:gold_ingot,minecraft:"
            "iron_ingot",
            R"(minecraft:iron_chestplatex1{"minecraft:trim":{pattern:"minecraft:coast",material:"minecraft:gold"}})");
    }
    SUBCASE("trimAgain") {
        checkSmithing("trim", stack("minecraft:coast_armor_trim_smithing_template"),
                      stack("minecraft:iron_chestplate", 1, alreadyTrimmed()), stack("minecraft:gold_ingot"), true,
                      "smithing/minecraft:coast/"
                      "minecraft:coast_armor_trim_smithing_template;minecraft:iron_chestplate;minecraft:gold_ingot,"
                      "minecraft:iron_ingot",
                      "-");
    }
    SUBCASE("trimNoMaterial") {
        checkSmithing("trim", stack("minecraft:coast_armor_trim_smithing_template"), stack("minecraft:iron_chestplate"),
                      stack("minecraft:copper_ingot"), false,
                      "smithing/minecraft:coast/"
                      "minecraft:coast_armor_trim_smithing_template;minecraft:iron_chestplate;minecraft:gold_ingot,"
                      "minecraft:iron_ingot",
                      "-");
    }
}

TEST_CASE("the parser refuses what the frozen kotlin refuses") {
    SUBCASE("unknownType") {
        CHECK(refusal(R"({"type":"minecraft:crafting_magic","result":{"id":"minecraft:stone"}})") ==
              "Unsupported_recipe_type:_minecraft:crafting_magic");
    }
    SUBCASE("longSymbol") {
        CHECK(refusal(R"({"type":"minecraft:crafting_shaped","pattern":["AB"],"key":{"AB":"minecraft:stone"},
               "result":{"id":"minecraft:stone"}})") == "Invalid_pattern_key_symbol_'AB':_must_be_1_character");
    }
    // parity: 26.3 dropped the per-type default, so a cooking recipe must say how long it takes.
    SUBCASE("noCookingTime") {
        CHECK(refusal(R"({"type":"minecraft:blasting","ingredient":"minecraft:iron_ore",
               "result":{"id":"minecraft:iron_ingot"}})") == "missing_or_malformed_cookingtime");
    }
    SUBCASE("undefinedSymbol") {
        CHECK(refusal(R"({"type":"minecraft:crafting_shaped","pattern":["AB"],"key":{"A":"minecraft:stone"},
               "result":{"id":"minecraft:stone"}})") ==
              "Pattern_references_symbol_'B'_but_it's_not_defined_in_the_key");
    }
}

TEST_CASE("a brewing recipe reads its bottle, the potion in it, the reagent and what comes out") {
    const std::expected<Recipe, RecipeParseError> parsed = recipeJson::parse(
        R"({"type":"minecraft:brewing","input":{"item":"minecraft:potion","potion_contents":{"potions":
           "minecraft:water"}},"reagent":{"item":"minecraft:nether_wart"},"output":{"id":"minecraft:potion",
           "components":{"minecraft:potion_contents":{"potion":"minecraft:awkward"}}}})",
        resolveTag);
    REQUIRE(parsed.has_value());
    CHECK(parsed->kind == RecipeKind::Brewing);
    CHECK(parsed->potions == std::vector<std::string>{"minecraft:water"});
    CHECK(parsed->result.item == Identifier::vanilla("potion"));
    const nbt::NbtCompound* contents = parsed->result.components.getCompound("minecraft:potion_contents");
    REQUIRE(contents != nullptr);
    CHECK(contents->getString("potion") == "minecraft:awkward");
}

#include <basalt/server/world/crafting/RecipeJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <charconv>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::crafting::recipeJson {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

template <typename Value>
using Parsed = std::expected<Value, RecipeParseError>;

[[nodiscard]] RecipeParseError missing(std::string_view what) {
    return RecipeParseError{std::string{"missing or malformed "} + std::string{what}};
}

[[nodiscard]] std::optional<std::string> asString(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const std::string* text = value->get<std::string>();
    return text == nullptr ? std::nullopt : std::optional<std::string>{*text};
}

[[nodiscard]] std::optional<double> asNumber(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const double* real = value->get<double>(); real != nullptr) {
        return *real;
    }
    if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        double parsed = 0.0;
        if (std::from_chars(text->data(), text->data() + text->size(), parsed).ec == std::errc{}) {
            return parsed;
        }
    }
    return std::nullopt;
}

/// A token is an item name or, with a leading `#`, a tag. Either way it is a resource ID, so it is
/// normalised before it is compared against a stack's own.
[[nodiscard]] Parsed<Ingredient> ingredientOfToken(std::string_view raw, const TagResolver& resolveTag) {
    const bool tag = !raw.empty() && raw.front() == '#';
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(tag ? raw.substr(1) : raw);
    if (!id.has_value()) {
        return std::unexpected(RecipeParseError{"Malformed ingredient token: " + std::string{raw}});
    }
    if (tag) {
        return Ingredient{resolveTag(id->toString())};
    }
    return Ingredient{{id->toString()}};
}

[[nodiscard]] Parsed<Ingredient> parseIngredient(const JsonValue& element, const TagResolver& resolveTag) {
    if (const std::string* token = element.get<std::string>(); token != nullptr) {
        return ingredientOfToken(*token, resolveTag);
    }
    if (const std::vector<JsonValue>* array = element.get<std::vector<JsonValue>>(); array != nullptr) {
        Ingredient built;
        for (const JsonValue& entry : *array) {
            const std::string* token = entry.get<std::string>();
            if (token == nullptr) {
                return std::unexpected(missing("ingredient token"));
            }
            Parsed<Ingredient> one = ingredientOfToken(*token, resolveTag);
            if (!one.has_value()) {
                return std::unexpected(one.error());
            }
            built.items.insert(one->items.begin(), one->items.end());
        }
        return built;
    }
    return std::unexpected(RecipeParseError{"Unsupported ingredient JSON"});
}

[[nodiscard]] Parsed<Ingredient> requiredIngredient(const JsonObject& root, std::string_view key,
                                                    const TagResolver& resolveTag) {
    const JsonValue* value = root.find(key);
    if (value == nullptr) {
        return std::unexpected(missing(key));
    }
    return parseIngredient(*value, resolveTag);
}

[[nodiscard]] Parsed<std::optional<Ingredient>> optionalIngredient(const JsonObject& root, std::string_view key,
                                                                   const TagResolver& resolveTag) {
    const JsonValue* value = root.find(key);
    if (value == nullptr) {
        return std::optional<Ingredient>{};
    }
    Parsed<Ingredient> parsed = parseIngredient(*value, resolveTag);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }
    return std::optional<Ingredient>{std::move(*parsed)};
}

/// parity: `ShapedRecipePattern.shrink`: trims blank leading and trailing rows and columns from the
/// authored pattern before it is turned into a fixed grid.
[[nodiscard]] std::vector<std::string> shrinkPattern(const std::vector<std::string>& pattern) {
    std::int32_t left = std::numeric_limits<std::int32_t>::max();
    std::int32_t right = 0;
    std::int32_t top = 0;
    std::int32_t bottom = 0;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const std::string& line = pattern[index];
        std::int32_t first = 0;
        while (first < static_cast<std::int32_t>(line.size()) && line[static_cast<std::size_t>(first)] == ' ') {
            ++first;
        }
        left = std::min(left, first);
        std::int32_t last = static_cast<std::int32_t>(line.size()) - 1;
        while (last >= 0 && line[static_cast<std::size_t>(last)] == ' ') {
            --last;
        }
        right = std::max(right, last);
        if (last < 0) {
            if (top == static_cast<std::int32_t>(index)) {
                ++top;
            }
            ++bottom;
        } else {
            bottom = 0;
        }
    }
    if (static_cast<std::int32_t>(pattern.size()) == bottom) {
        return {};
    }
    std::vector<std::string> shrunk;
    for (std::int32_t line = 0; line < static_cast<std::int32_t>(pattern.size()) - bottom - top; ++line) {
        shrunk.push_back(pattern[static_cast<std::size_t>(line + top)].substr(
            static_cast<std::size_t>(left), static_cast<std::size_t>(right - left + 1)));
    }
    return shrunk;
}

[[nodiscard]] Parsed<Recipe> parseShaped(const JsonObject& root, Recipe built, const TagResolver& resolveTag) {
    const JsonObject* key = root.find("key") == nullptr ? nullptr : root.find("key")->get<JsonObject>();
    if (key == nullptr) {
        return std::unexpected(missing("shaped key"));
    }
    std::map<char, Ingredient> symbols;
    for (std::size_t index = 0; index < key->names().size(); ++index) {
        const std::string& symbol = key->names()[index];
        if (symbol.size() != 1) {
            return std::unexpected(
                RecipeParseError{"Invalid pattern key symbol '" + symbol + "': must be 1 character"});
        }
        Parsed<Ingredient> ingredient = parseIngredient(key->values()[index], resolveTag);
        if (!ingredient.has_value()) {
            return std::unexpected(ingredient.error());
        }
        symbols.emplace(symbol.front(), std::move(*ingredient));
    }
    const JsonValue* patternValue = root.find("pattern");
    const std::vector<JsonValue>* rows =
        patternValue == nullptr ? nullptr : patternValue->get<std::vector<JsonValue>>();
    if (rows == nullptr) {
        return std::unexpected(missing("shaped pattern"));
    }
    std::vector<std::string> lines;
    for (const JsonValue& row : *rows) {
        const std::optional<std::string> line = asString(&row);
        if (!line.has_value()) {
            return std::unexpected(missing("pattern row"));
        }
        lines.push_back(*line);
    }
    const std::vector<std::string> pattern = shrinkPattern(lines);
    if (pattern.empty()) {
        return std::unexpected(missing("shaped pattern"));
    }
    built.kind = RecipeKind::Shaped;
    built.width = static_cast<std::int32_t>(pattern.front().size());
    built.height = static_cast<std::int32_t>(pattern.size());
    for (const std::string& line : pattern) {
        for (const char symbol : line) {
            if (symbol == ' ') {
                built.shape.emplace_back();
                continue;
            }
            const auto found = symbols.find(symbol);
            if (found == symbols.end()) {
                return std::unexpected(RecipeParseError{std::string{"Pattern references symbol '"} + symbol +
                                                        "' but it's not defined in the key"});
            }
            built.shape.emplace_back(found->second);
        }
    }
    return built;
}

[[nodiscard]] Parsed<Recipe> parseCooking(const JsonObject& root, Recipe built, Cooker cooker,
                                          std::int32_t defaultCookingTime, const TagResolver& resolveTag) {
    Parsed<Ingredient> ingredient = requiredIngredient(root, "ingredient", resolveTag);
    if (!ingredient.has_value()) {
        return std::unexpected(ingredient.error());
    }
    built.kind = RecipeKind::Cooking;
    built.cooker = cooker;
    built.ingredient = std::move(*ingredient);
    built.experience = static_cast<float>(asNumber(root.find("experience")).value_or(0.0));
    built.cookingTime = static_cast<std::int32_t>(asNumber(root.find("cookingtime")).value_or(defaultCookingTime));
    return built;
}

/// One special recipe, with whichever of its two ingredient fields the file carries.
[[nodiscard]] Parsed<Recipe> parseSpecial(const JsonObject& root, Recipe built, SpecialKind specialKind,
                                          std::string_view targetKey, std::string_view materialKey,
                                          const TagResolver& resolveTag) {
    built.kind = RecipeKind::Special;
    built.specialKind = specialKind;
    if (!targetKey.empty()) {
        Parsed<std::optional<Ingredient>> target = optionalIngredient(root, targetKey, resolveTag);
        if (!target.has_value()) {
            return std::unexpected(target.error());
        }
        built.target = std::move(*target);
    }
    if (!materialKey.empty()) {
        Parsed<std::optional<Ingredient>> material = optionalIngredient(root, materialKey, resolveTag);
        if (!material.has_value()) {
            return std::unexpected(material.error());
        }
        built.material = std::move(*material);
    }
    return built;
}

/// parity: `FireworkStarRecipe`'s four shapes plus its trail and twinkle, each named.
[[nodiscard]] Parsed<std::vector<std::pair<std::string, Ingredient>>>
parseFireworkModifiers(const JsonObject& root, const TagResolver& resolveTag) {
    std::vector<std::pair<std::string, Ingredient>> modifiers;
    if (const JsonValue* shapesValue = root.find("shapes"); shapesValue != nullptr) {
        if (const JsonObject* shapes = shapesValue->get<JsonObject>(); shapes != nullptr) {
            for (std::size_t index = 0; index < shapes->names().size(); ++index) {
                Parsed<Ingredient> ingredient = parseIngredient(shapes->values()[index], resolveTag);
                if (!ingredient.has_value()) {
                    return std::unexpected(ingredient.error());
                }
                modifiers.emplace_back(shapes->names()[index], std::move(*ingredient));
            }
        }
    }
    for (const std::string_view key : {"trail", "twinkle"}) {
        if (const JsonValue* value = root.find(key); value != nullptr) {
            Parsed<Ingredient> ingredient = parseIngredient(*value, resolveTag);
            if (!ingredient.has_value()) {
                return std::unexpected(ingredient.error());
            }
            modifiers.emplace_back(std::string{key}, std::move(*ingredient));
        }
    }
    return modifiers;
}

} // namespace

std::expected<Recipe, RecipeParseError> parse(std::string_view json, const TagResolver& resolveTag) {
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::unexpected(RecipeParseError{"Malformed recipe JSON"});
    }
    const JsonObject* root = parsed->get<JsonObject>();
    if (root == nullptr) {
        return std::unexpected(missing("recipe object"));
    }
    const std::optional<std::string> type = asString(root->find("type"));
    if (!type.has_value()) {
        return std::unexpected(missing("recipe type"));
    }

    // parity: two recipe types carry no result at all, because theirs comes out of the grid: a trim
    // is the armour that went in, and a repair is the tool that went in.
    Recipe built{.result = loot::ItemStack{.item = wire::common::Identifier::vanilla("air"), .count = 1}};
    built.group = asString(root->find("group")).value_or("");
    if (const JsonValue* resultValue = root->find("result"); resultValue != nullptr) {
        if (const JsonObject* result = resultValue->get<JsonObject>(); result != nullptr) {
            const std::optional<std::string> id = asString(result->find("id"));
            if (!id.has_value()) {
                return std::unexpected(missing("result id"));
            }
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
                wire::common::Identifier::parse(*id);
            if (!item.has_value()) {
                return std::unexpected(RecipeParseError{"Malformed result id: " + *id});
            }
            built.result = loot::ItemStack{
                .item = *item, .count = static_cast<std::int32_t>(asNumber(result->find("count")).value_or(1.0))};
        }
    }

    if (*type == "minecraft:crafting_shaped") {
        return parseShaped(*root, std::move(built), resolveTag);
    }
    if (*type == "minecraft:crafting_shapeless") {
        const JsonValue* value = root->find("ingredients");
        const std::vector<JsonValue>* array = value == nullptr ? nullptr : value->get<std::vector<JsonValue>>();
        if (array == nullptr) {
            return std::unexpected(missing("shapeless ingredients"));
        }
        built.kind = RecipeKind::Shapeless;
        for (const JsonValue& element : *array) {
            Parsed<Ingredient> ingredient = parseIngredient(element, resolveTag);
            if (!ingredient.has_value()) {
                return std::unexpected(ingredient.error());
            }
            built.ingredients.push_back(std::move(*ingredient));
        }
        return built;
    }
    // parity: each cooking type's own default in `cookingMapCodec`, which is what a recipe file
    // leaving `cookingtime` out gets.
    if (*type == "minecraft:smelting") {
        return parseCooking(*root, std::move(built), Cooker::Smelting, 200, resolveTag);
    }
    if (*type == "minecraft:blasting") {
        return parseCooking(*root, std::move(built), Cooker::Blasting, 100, resolveTag);
    }
    if (*type == "minecraft:smoking") {
        return parseCooking(*root, std::move(built), Cooker::Smoking, 100, resolveTag);
    }
    if (*type == "minecraft:campfire_cooking") {
        return parseCooking(*root, std::move(built), Cooker::Campfire, 100, resolveTag);
    }
    // parity: `TransmuteRecipe`: two ingredients in any arrangement, one of which decides what comes
    // out. The result keeps the input's components in vanilla; nothing here carries components
    // through a craft yet, so it comes out plain.
    if (*type == "minecraft:crafting_transmute") {
        Parsed<Ingredient> input = requiredIngredient(*root, "input", resolveTag);
        if (!input.has_value()) {
            return std::unexpected(input.error());
        }
        Parsed<Ingredient> material = requiredIngredient(*root, "material", resolveTag);
        if (!material.has_value()) {
            return std::unexpected(material.error());
        }
        built.kind = RecipeKind::Shapeless;
        built.ingredients = {std::move(*input), std::move(*material)};
        return built;
    }
    if (*type == "minecraft:stonecutting") {
        Parsed<Ingredient> ingredient = requiredIngredient(*root, "ingredient", resolveTag);
        if (!ingredient.has_value()) {
            return std::unexpected(ingredient.error());
        }
        built.kind = RecipeKind::Stonecutting;
        built.ingredient = std::move(*ingredient);
        return built;
    }
    if (*type == "minecraft:smithing_transform" || *type == "minecraft:smithing_trim") {
        Parsed<Ingredient> smithingTemplate = requiredIngredient(*root, "template", resolveTag);
        if (!smithingTemplate.has_value()) {
            return std::unexpected(smithingTemplate.error());
        }
        Parsed<Ingredient> base = requiredIngredient(*root, "base", resolveTag);
        if (!base.has_value()) {
            return std::unexpected(base.error());
        }
        Parsed<Ingredient> addition = requiredIngredient(*root, "addition", resolveTag);
        if (!addition.has_value()) {
            return std::unexpected(addition.error());
        }
        built.kind = RecipeKind::Smithing;
        built.smithingTemplate = std::move(*smithingTemplate);
        built.base = std::move(*base);
        built.addition = std::move(*addition);
        if (*type == "minecraft:smithing_trim") {
            const std::optional<std::string> pattern = asString(root->find("pattern"));
            if (!pattern.has_value()) {
                return std::unexpected(missing("trim pattern"));
            }
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsedPattern =
                wire::common::Identifier::parse(*pattern);
            if (!parsedPattern.has_value()) {
                return std::unexpected(RecipeParseError{"Malformed trim pattern: " + *pattern});
            }
            built.trimPattern = *parsedPattern;
        }
        return built;
    }
    // parity: the specials, which name their own ingredients in 26.2: what is code is the rule, not
    // the list of items.
    if (*type == "minecraft:crafting_dye") {
        return parseSpecial(*root, std::move(built), SpecialKind::Dye, "target", "dye", resolveTag);
    }
    if (*type == "minecraft:crafting_imbue") {
        return parseSpecial(*root, std::move(built), SpecialKind::Imbue, "source", "material", resolveTag);
    }
    if (*type == "minecraft:crafting_decorated_pot") {
        return parseSpecial(*root, std::move(built), SpecialKind::DecoratedPot, "", "back", resolveTag);
    }
    if (*type == "minecraft:crafting_special_bookcloning") {
        return parseSpecial(*root, std::move(built), SpecialKind::BookCloning, "source", "material", resolveTag);
    }
    if (*type == "minecraft:crafting_special_bannerduplicate") {
        return parseSpecial(*root, std::move(built), SpecialKind::BannerDuplicate, "banner", "", resolveTag);
    }
    if (*type == "minecraft:crafting_special_shielddecoration") {
        return parseSpecial(*root, std::move(built), SpecialKind::ShieldDecoration, "target", "banner", resolveTag);
    }
    if (*type == "minecraft:crafting_special_mapextending") {
        return parseSpecial(*root, std::move(built), SpecialKind::MapExtending, "map", "material", resolveTag);
    }
    if (*type == "minecraft:crafting_special_firework_rocket") {
        Parsed<Recipe> rocket =
            parseSpecial(*root, std::move(built), SpecialKind::FireworkRocket, "shell", "fuel", resolveTag);
        if (!rocket.has_value()) {
            return rocket;
        }
        Parsed<std::optional<Ingredient>> star = optionalIngredient(*root, "star", resolveTag);
        if (!star.has_value()) {
            return std::unexpected(star.error());
        }
        rocket->extra = std::move(*star);
        return rocket;
    }
    if (*type == "minecraft:crafting_special_firework_star") {
        Parsed<Recipe> star =
            parseSpecial(*root, std::move(built), SpecialKind::FireworkStar, "fuel", "dye", resolveTag);
        if (!star.has_value()) {
            return star;
        }
        Parsed<std::vector<std::pair<std::string, Ingredient>>> modifiers = parseFireworkModifiers(*root, resolveTag);
        if (!modifiers.has_value()) {
            return std::unexpected(modifiers.error());
        }
        star->modifiers = std::move(*modifiers);
        return star;
    }
    if (*type == "minecraft:crafting_special_firework_star_fade") {
        return parseSpecial(*root, std::move(built), SpecialKind::FireworkStarFade, "target", "dye", resolveTag);
    }
    // parity: `RepairItemRecipe`, the one that names nothing because both its inputs are whatever
    // damaged item you put in.
    if (*type == "minecraft:crafting_special_repairitem") {
        built.kind = RecipeKind::Special;
        built.specialKind = SpecialKind::RepairItem;
        return built;
    }
    return std::unexpected(RecipeParseError{"Unsupported recipe type: " + *type});
}

} // namespace basalt::server::world::crafting::recipeJson

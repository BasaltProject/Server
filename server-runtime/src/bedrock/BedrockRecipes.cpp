#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

namespace crafting = server::world::crafting;

constexpr std::string_view kStonecutterTag = "stonecutter";

constexpr std::size_t kMaxVariants = 24;

// The item tags that edition has under the same name; any other tag means nothing to its client.
constexpr std::array<std::string_view, 9> kSharedTags{"minecraft:planks",
                                                      "minecraft:logs",
                                                      "minecraft:coals",
                                                      "minecraft:wooden_slabs",
                                                      "minecraft:wool",
                                                      "minecraft:boats",
                                                      "minecraft:stone_crafting_materials",
                                                      "minecraft:stone_tool_materials",
                                                      "minecraft:soul_fire_base_blocks"};

[[nodiscard]] wire::bedrock::BedrockIngredient singleItem(const std::string& java) {
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(java);
    if (named == nullptr) {
        return {};
    }
    return wire::bedrock::BedrockIngredient{
        .name = wire::bedrock::bedrockItemPalette()[static_cast<std::size_t>(named->palettePlace)],
        .aux = named->data,
        .count = 1};
}

// An ingredient of several items has no spelling over there unless a tag is shared: each item becomes a recipe.
[[nodiscard]] std::vector<wire::bedrock::BedrockIngredient> optionsOf(const crafting::Ingredient& ingredient,
                                                                      const bool expand) {
    if (ingredient.items.empty()) {
        return {wire::bedrock::BedrockIngredient{}};
    }
    if (ingredient.items.size() > 1 && std::ranges::find(kSharedTags, ingredient.tag) != kSharedTags.end()) {
        return {wire::bedrock::BedrockIngredient{.name = ingredient.tag, .tag = true, .count = 1}};
    }
    if (ingredient.items.size() == 1 || !expand) {
        return {singleItem(*ingredient.items.begin())};
    }
    std::vector<wire::bedrock::BedrockIngredient> out;
    for (const std::string& item : ingredient.items) {
        if (wire::bedrock::BedrockIngredient one = singleItem(item); !one.name.empty()) {
            out.push_back(std::move(one));
        }
    }
    if (out.empty()) {
        out.emplace_back();
    }
    return out;
}

// Every way to pick one item per distinct ingredient; the same ingredient in two cells takes the same pick.
[[nodiscard]] std::vector<std::vector<wire::bedrock::BedrockIngredient>>
variantsOf(const std::vector<std::optional<crafting::Ingredient>>& cells) {
    std::vector<const crafting::Ingredient*> distinct;
    std::vector<std::size_t> kind(cells.size(), SIZE_MAX);
    for (std::size_t at = 0; at < cells.size(); ++at) {
        if (!cells[at].has_value()) {
            continue;
        }
        const auto found =
            std::ranges::find_if(distinct, [&](const crafting::Ingredient* seen) { return *seen == *cells[at]; });
        kind[at] = static_cast<std::size_t>(found - distinct.begin());
        if (found == distinct.end()) {
            distinct.push_back(&*cells[at]);
        }
    }
    const auto build = [&](const bool expand) {
        std::vector<std::vector<wire::bedrock::BedrockIngredient>> options;
        std::size_t product = 1;
        for (const crafting::Ingredient* one : distinct) {
            options.push_back(optionsOf(*one, expand));
            product *= options.back().size();
        }
        return std::pair{std::move(options), product};
    };
    auto [options, product] = build(true);
    if (product > kMaxVariants) {
        std::tie(options, product) = build(false);
    }
    std::vector<std::vector<wire::bedrock::BedrockIngredient>> out;
    std::vector<std::size_t> pick(distinct.size(), 0);
    for (std::size_t variant = 0; variant < product; ++variant) {
        std::vector<wire::bedrock::BedrockIngredient> filled;
        filled.reserve(cells.size());
        for (std::size_t at = 0; at < cells.size(); ++at) {
            filled.push_back(kind[at] == SIZE_MAX ? wire::bedrock::BedrockIngredient{}
                                                  : options[kind[at]][pick[kind[at]]]);
        }
        out.push_back(std::move(filled));
        for (std::size_t digit = 0; digit < pick.size(); ++digit) {
            if (++pick[digit] < options[digit].size()) {
                break;
            }
            pick[digit] = 0;
        }
    }
    return out;
}

[[nodiscard]] wire::bedrock::BedrockItemStack resultOf(const server::world::loot::ItemStack& stack) {
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(stack.item.toString());
    if (named == nullptr) {
        return {};
    }
    return wire::bedrock::BedrockItemStack{
        .runtimeId = named->palettePlace + 1, .count = stack.count, .data = named->data};
}

// One unknown item makes the recipe unsendable: a cell the client cannot read is worse than a missing recipe.
[[nodiscard]] bool sendable(const wire::bedrock::BedrockRecipe& recipe) {
    if (recipe.result.runtimeId == 0 || recipe.result.count <= 0) {
        return false;
    }
    for (const wire::bedrock::BedrockIngredient& ingredient : recipe.ingredients) {
        if (ingredient.name.empty() && !recipe.shaped) {
            return false;
        }
    }
    return true;
}

} // namespace

BedrockRecipeList bedrockRecipes(const world::RealRecipes& recipes) {
    BedrockRecipeList out;
    std::int32_t netId = 1;
    const auto add = [&](wire::bedrock::BedrockRecipe built, const server::world::loot::ItemStack& result) {
        if (!sendable(built)) {
            ++out.unnamed;
            return;
        }
        built.netId = netId;
        out.results.emplace(netId, storage::StoredItemStack{.id = result.item.toString(), .count = result.count});
        ++netId;
        out.recipes.push_back(std::move(built));
    };
    for (const wire::common::Identifier& id : recipes.ids()) {
        const crafting::Recipe* recipe = recipes.recipeFor(id);
        if (recipe == nullptr ||
            (recipe->kind != crafting::RecipeKind::Shaped && recipe->kind != crafting::RecipeKind::Shapeless)) {
            continue;
        }
        // A copied map carries the data of the one it copies, which a recipe cannot name.
        if (recipe->result.item.toString() == "minecraft:filled_map") {
            continue;
        }
        std::vector<std::optional<crafting::Ingredient>> cells;
        if (recipe->kind == crafting::RecipeKind::Shaped) {
            cells = recipe->shape;
        } else {
            cells.assign(recipe->ingredients.begin(), recipe->ingredients.end());
        }
        std::size_t variant = 0;
        for (std::vector<wire::bedrock::BedrockIngredient>& filled : variantsOf(cells)) {
            wire::bedrock::BedrockRecipe built;
            built.id = variant == 0 ? id.toString() : id.toString() + "_" + std::to_string(variant);
            ++variant;
            built.result = resultOf(recipe->result);
            built.shaped = recipe->kind == crafting::RecipeKind::Shaped;
            built.width = recipe->width;
            built.height = recipe->height;
            built.ingredients = std::move(filled);
            add(std::move(built), recipe->result);
        }
    }
    for (const crafting::Recipe* recipe : recipes.stonecutting()) {
        for (std::vector<wire::bedrock::BedrockIngredient>& filled :
             variantsOf({std::optional<crafting::Ingredient>{recipe->ingredient}})) {
            wire::bedrock::BedrockRecipe built;
            built.id = "minecraft:stonecutter_" + std::to_string(netId);
            built.tag = kStonecutterTag;
            built.ingredients = std::move(filled);
            built.result = resultOf(recipe->result);
            add(std::move(built), recipe->result);
        }
    }
    return out;
}

wire::bedrock::BedrockTradeItem tradeItemOf(const server::world::loot::ItemStack& stack) {
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(stack.item.toString());
    if (named == nullptr) {
        return {};
    }
    return wire::bedrock::BedrockTradeItem{
        .name = wire::bedrock::bedrockItemPalette()[static_cast<std::size_t>(named->palettePlace)],
        .count = stack.count,
        .damage = named->data};
}

} // namespace basalt::server::runtime::bedrock

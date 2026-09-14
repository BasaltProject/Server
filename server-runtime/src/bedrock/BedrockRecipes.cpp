#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>

#include <string>

namespace basalt::server::runtime::bedrock {
namespace {

namespace crafting = server::world::crafting;

/// What one ingredient is on the other edition: the tag it came from when it had one, and the item
/// itself when it did not. An ingredient of several items with no tag behind it can only be sent as
/// one of them, which is the one thing here that is not a faithful translation.
[[nodiscard]] wire::bedrock::BedrockIngredient ingredientOf(const crafting::Ingredient& ingredient) {
    if (ingredient.items.empty()) {
        return {};
    }
    if (!ingredient.tag.empty() && ingredient.items.size() > 1) {
        return wire::bedrock::BedrockIngredient{.name = ingredient.tag, .tag = true, .count = 1};
    }
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(*ingredient.items.begin());
    if (named == nullptr) {
        return {};
    }
    return wire::bedrock::BedrockIngredient{
        .name = wire::bedrock::bedrockItemPalette()[static_cast<std::size_t>(named->palettePlace)],
        .aux = named->data,
        .count = 1};
}

/// The stack a recipe makes, by the number this server gave that item at login.
[[nodiscard]] wire::bedrock::BedrockItemStack resultOf(const server::world::loot::ItemStack& stack) {
    const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(stack.item.toString());
    if (named == nullptr) {
        return {};
    }
    return wire::bedrock::BedrockItemStack{
        .runtimeId = named->palettePlace + 1, .count = stack.count, .data = named->data};
}

/// Whether every cell of this recipe is something that edition has a name for. One unknown item
/// makes the whole recipe unsendable: a cell the client cannot read is worse than a missing recipe.
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
    for (const wire::common::Identifier& id : recipes.ids()) {
        const crafting::Recipe* recipe = recipes.recipeFor(id);
        if (recipe == nullptr ||
            (recipe->kind != crafting::RecipeKind::Shaped && recipe->kind != crafting::RecipeKind::Shapeless)) {
            continue;
        }
        wire::bedrock::BedrockRecipe built;
        built.id = id.toString();
        built.result = resultOf(recipe->result);
        built.netId = netId;
        if (recipe->kind == crafting::RecipeKind::Shaped) {
            built.shaped = true;
            built.width = recipe->width;
            built.height = recipe->height;
            built.ingredients.reserve(recipe->shape.size());
            for (const std::optional<crafting::Ingredient>& cell : recipe->shape) {
                built.ingredients.push_back(cell.has_value() ? ingredientOf(*cell)
                                                             : wire::bedrock::BedrockIngredient{});
            }
        } else {
            built.ingredients.reserve(recipe->ingredients.size());
            for (const crafting::Ingredient& ingredient : recipe->ingredients) {
                built.ingredients.push_back(ingredientOf(ingredient));
            }
        }
        if (!sendable(built)) {
            ++out.unnamed;
            continue;
        }
        ++netId;
        out.recipes.push_back(std::move(built));
    }
    return out;
}

} // namespace basalt::server::runtime::bedrock

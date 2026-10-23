#include <basalt/server/runtime/plugins/PluginRecipeNbt.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins {
namespace {

namespace crafting = server::world::crafting;
namespace loot = server::world::loot;

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] NbtCompound stackToNbt(const loot::ItemStack& stack) {
    NbtCompound out;
    out.putString("id", stack.item.toString());
    out.putInt("count", stack.count);
    out.set("components", NbtTag::ofCompound(stack.components));
    return out;
}

[[nodiscard]] std::optional<loot::ItemStack> stackFromNbt(const NbtCompound& data) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(data.getString("id"));
    if (!id.has_value()) {
        return std::nullopt;
    }
    loot::ItemStack stack{.item = *id, .count = data.getInt("count", 1)};
    if (const NbtCompound* components = data.getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

[[nodiscard]] NbtCompound ingredientToNbt(const crafting::Ingredient& ingredient) {
    NbtCompound out;
    std::vector<NbtTag> items;
    for (const std::string& item : ingredient.items) {
        items.push_back(NbtTag::ofString(item));
    }
    out.set("items", NbtTag::ofList(NbtList{NbtType::String, std::move(items)}));
    out.putString("tag", ingredient.tag);
    std::vector<NbtTag> exact;
    for (const loot::ItemStack& stack : ingredient.exact) {
        exact.push_back(NbtTag::ofCompound(stackToNbt(stack)));
    }
    out.set("exact", NbtTag::ofList(NbtList{NbtType::Compound, std::move(exact)}));
    return out;
}

[[nodiscard]] crafting::Ingredient ingredientFromNbt(const NbtCompound& data) {
    crafting::Ingredient out;
    if (const NbtList* items = data.getList("items"); items != nullptr) {
        for (const NbtTag& item : *items) {
            if (const std::string* text = item.get<std::string>(); text != nullptr) {
                out.items.insert(*text);
            }
        }
    }
    out.tag = std::string{data.getString("tag")};
    if (const NbtList* exact = data.getList("exact"); exact != nullptr) {
        for (const NbtTag& entry : *exact) {
            if (const NbtCompound* compound = entry.get<NbtCompound>(); compound != nullptr) {
                if (std::optional<loot::ItemStack> stack = stackFromNbt(*compound); stack.has_value()) {
                    out.exact.push_back(std::move(*stack));
                }
            }
        }
    }
    return out;
}

[[nodiscard]] const NbtCompound* compoundIn(const NbtCompound& data, std::string_view key) {
    return data.getCompound(key);
}

} // namespace

NbtCompound recipeToNbt(const crafting::Recipe& recipe) {
    NbtCompound out;
    out.putInt("kind", static_cast<std::int32_t>(recipe.kind));
    out.putString("id", recipe.id);
    out.putString("group", recipe.group);
    out.set("result", NbtTag::ofCompound(stackToNbt(recipe.result)));
    out.putInt("width", recipe.width);
    out.putInt("height", recipe.height);
    std::vector<NbtTag> shape;
    for (const std::optional<crafting::Ingredient>& cell : recipe.shape) {
        shape.push_back(NbtTag::ofCompound(cell.has_value() ? ingredientToNbt(*cell) : NbtCompound{}));
    }
    out.set("shape", NbtTag::ofList(NbtList{NbtType::Compound, std::move(shape)}));
    std::vector<NbtTag> ingredients;
    for (const crafting::Ingredient& ingredient : recipe.ingredients) {
        ingredients.push_back(NbtTag::ofCompound(ingredientToNbt(ingredient)));
    }
    out.set("ingredients", NbtTag::ofList(NbtList{NbtType::Compound, std::move(ingredients)}));
    out.set("ingredient", NbtTag::ofCompound(ingredientToNbt(recipe.ingredient)));
    out.putInt("cooker", static_cast<std::int32_t>(recipe.cooker));
    out.putFloat("experience", recipe.experience);
    out.putInt("cooking_time", recipe.cookingTime);
    out.set("template", NbtTag::ofCompound(ingredientToNbt(recipe.smithingTemplate)));
    out.set("base", NbtTag::ofCompound(ingredientToNbt(recipe.base)));
    out.set("addition", NbtTag::ofCompound(ingredientToNbt(recipe.addition)));
    if (recipe.trimPattern.has_value()) {
        out.putString("trim_pattern", recipe.trimPattern->toString());
    }
    out.putBoolean("transmute", recipe.transmute);
    out.putInt("material_min", recipe.materialMin);
    out.putInt("material_max", recipe.materialMax);
    out.putBoolean("add_material_count", recipe.addMaterialCount);
    out.putInt("special_kind", static_cast<std::int32_t>(recipe.specialKind));
    return out;
}

std::optional<crafting::Recipe> recipeFromNbt(const NbtCompound& data) {
    crafting::Recipe recipe{.result = loot::ItemStack{.item = wire::common::Identifier::vanilla("air"), .count = 1}};
    const std::int32_t kind = data.getInt("kind", -1);
    if (kind < 0 || kind > static_cast<std::int32_t>(crafting::RecipeKind::Brewing)) {
        return std::nullopt;
    }
    recipe.kind = static_cast<crafting::RecipeKind>(kind);
    recipe.group = std::string{data.getString("group")};
    if (const NbtCompound* result = compoundIn(data, "result"); result != nullptr) {
        std::optional<loot::ItemStack> stack = stackFromNbt(*result);
        if (!stack.has_value()) {
            return std::nullopt;
        }
        recipe.result = std::move(*stack);
    }
    recipe.width = data.getInt("width");
    recipe.height = data.getInt("height");
    if (const NbtList* shape = data.getList("shape"); shape != nullptr) {
        for (const NbtTag& cell : *shape) {
            const NbtCompound* compound = cell.get<NbtCompound>();
            if (compound == nullptr || compound->empty()) {
                recipe.shape.emplace_back(std::nullopt);
            } else {
                recipe.shape.emplace_back(ingredientFromNbt(*compound));
            }
        }
    }
    if (const NbtList* ingredients = data.getList("ingredients"); ingredients != nullptr) {
        for (const NbtTag& entry : *ingredients) {
            if (const NbtCompound* compound = entry.get<NbtCompound>(); compound != nullptr) {
                recipe.ingredients.push_back(ingredientFromNbt(*compound));
            }
        }
    }
    if (const NbtCompound* ingredient = compoundIn(data, "ingredient"); ingredient != nullptr) {
        recipe.ingredient = ingredientFromNbt(*ingredient);
    }
    const std::int32_t cooker = data.getInt("cooker");
    recipe.cooker = static_cast<crafting::Cooker>(std::clamp(cooker, 0, 3));
    recipe.experience = data.getFloat("experience");
    recipe.cookingTime = data.getInt("cooking_time");
    if (const NbtCompound* value = compoundIn(data, "template"); value != nullptr) {
        recipe.smithingTemplate = ingredientFromNbt(*value);
    }
    if (const NbtCompound* value = compoundIn(data, "base"); value != nullptr) {
        recipe.base = ingredientFromNbt(*value);
    }
    if (const NbtCompound* value = compoundIn(data, "addition"); value != nullptr) {
        recipe.addition = ingredientFromNbt(*value);
    }
    if (data.contains("trim_pattern")) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> pattern =
            wire::common::Identifier::parse(data.getString("trim_pattern"));
        if (pattern.has_value()) {
            recipe.trimPattern = *pattern;
        }
    }
    recipe.transmute = data.getBoolean("transmute");
    recipe.materialMin = data.getInt("material_min", 1);
    recipe.materialMax = data.getInt("material_max", 1);
    recipe.addMaterialCount = data.getBoolean("add_material_count");
    recipe.specialKind = static_cast<crafting::SpecialKind>(data.getInt("special_kind"));
    return recipe;
}

} // namespace basalt::server::runtime::plugins

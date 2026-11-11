// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.item.crafting.CraftingBookCategory;
import net.minecraft.world.item.crafting.CraftingRecipe;
import net.minecraft.world.item.crafting.Recipe;

public interface RecipeBuilder {
    Identifier ROOT_RECIPE_ADVANCEMENT = Identifier.withDefaultNamespace("recipes/root");

    RecipeBuilder unlockedBy(String name, Criterion<?> criterion);

    RecipeBuilder group(String group);

    ResourceKey<Recipe<?>> defaultId();

    void save(RecipeOutput output, ResourceKey<Recipe<?>> location);

    default void save(RecipeOutput output) {
        save(output, defaultId());
    }

    default void save(RecipeOutput output, String id) {
        Identifier chosen = Identifier.parse(id);
        if (chosen.equals(defaultId().identifier())) {
            throw new IllegalStateException("Recipe " + id + " should remove its 'save' argument as it is equal to default one");
        }
        save(output, recipeKey(chosen));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static ResourceKey<Recipe<?>> recipeKey(Identifier id) {
        return (ResourceKey) ResourceKey.create((ResourceKey) Registries.RECIPE, id);
    }

    static CraftingBookCategory determineCraftingBookCategory(RecipeCategory category) {
        return switch (category) {
            case BUILDING_BLOCKS -> CraftingBookCategory.BUILDING;
            case TOOLS, COMBAT -> CraftingBookCategory.EQUIPMENT;
            case REDSTONE -> CraftingBookCategory.REDSTONE;
            default -> CraftingBookCategory.MISC;
        };
    }

    static Recipe.CommonInfo createCraftingCommonInfo(boolean showNotification) {
        return new Recipe.CommonInfo(showNotification);
    }

    static CraftingRecipe.CraftingBookInfo createCraftingBookInfo(RecipeCategory category, String group) {
        return new CraftingRecipe.CraftingBookInfo(determineCraftingBookCategory(category), group);
    }

    static ResourceKey<Recipe<?>> getDefaultRecipeId(ItemInstance result) {
        return recipeKey(RecipeJson.itemId(result.typeHolder().value()));
    }
}

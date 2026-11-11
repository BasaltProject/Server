// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import java.util.function.BiFunction;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.CraftingRecipe;
import net.minecraft.world.item.crafting.Recipe;

public class CustomCraftingRecipeBuilder {
    private final RecipeCategory category;
    private String groupName = "";

    public CustomCraftingRecipeBuilder(RecipeCategory category, Factory factory) {
        this.category = category;
    }

    public static CustomCraftingRecipeBuilder customCrafting(RecipeCategory category, Factory factory) {
        return new CustomCraftingRecipeBuilder(category, factory);
    }

    public CustomCraftingRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    public CustomCraftingRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    public void save(RecipeOutput output, String name) {
        save(output, RecipeBuilder.recipeKey(Identifier.withDefaultNamespace(name)));
    }

    public void save(RecipeOutput output, ResourceKey<Recipe<?>> id) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:crafting_" + id.identifier().getPath());
        json.addProperty("category", RecipeJson.craftingCategory(category));
        if (!groupName.isEmpty()) {
            json.addProperty("group", groupName);
        }
        RecipeJson.emit(output, id.identifier(), json);
    }

    public interface Factory extends BiFunction<Recipe.CommonInfo, CraftingRecipe.CraftingBookInfo, Recipe<?>> {
    }
}

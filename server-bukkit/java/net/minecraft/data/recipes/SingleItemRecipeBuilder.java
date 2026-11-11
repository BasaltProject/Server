// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.ItemLike;

public class SingleItemRecipeBuilder extends JsonRecipeBuilder {
    private final Ingredient ingredient;
    private final ItemLike result;
    private final int count;

    private SingleItemRecipeBuilder(RecipeCategory category, Ingredient ingredient, ItemLike result, int count) {
        super(category);
        this.ingredient = ingredient;
        this.result = result;
        this.count = count;
    }

    public static SingleItemRecipeBuilder stonecutting(Ingredient ingredient, RecipeCategory category, ItemLike result, int count) {
        return new SingleItemRecipeBuilder(category, ingredient, result, count);
    }

    @Override
    public SingleItemRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public SingleItemRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        return RecipeBuilder.recipeKey(RecipeJson.itemId(result));
    }

    @Override
    protected String type() {
        return "stonecutting";
    }

    @Override
    protected void fill(JsonObject json) {
        putGroup(json);
        json.add("ingredient", RecipeJson.ingredient(ingredient));
        json.add("result", RecipeJson.result(result, count));
    }
}

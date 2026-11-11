// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.TransmuteResult;

public class TransmuteRecipeBuilder extends JsonRecipeBuilder {
    private final Ingredient input;
    private final Ingredient material;
    private final Item result;
    private final int resultCount;
    private int materialCount = 1;

    private TransmuteRecipeBuilder(RecipeCategory category, Ingredient input, Ingredient material, Item result, int count) {
        super(category);
        this.input = input;
        this.material = material;
        this.result = result;
        this.resultCount = count;
    }

    public static TransmuteRecipeBuilder transmute(RecipeCategory category, Ingredient input, Ingredient material, Item result) {
        return new TransmuteRecipeBuilder(category, input, material, result, 1);
    }

    public static TransmuteRecipeBuilder transmute(RecipeCategory category, Ingredient input, Ingredient material, ItemStackTemplate result) {
        return new TransmuteRecipeBuilder(category, input, material, result.item().value(), result.count());
    }

    public static TransmuteRecipeBuilder transmute(RecipeCategory category, Ingredient input, Ingredient material, TransmuteResult result) {
        return new TransmuteRecipeBuilder(category, input, material, result.item().orElseThrow().value(), result.count());
    }

    @Override
    public TransmuteRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public TransmuteRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    public TransmuteRecipeBuilder addMaterialCountToOutput() {
        return this;
    }

    public TransmuteRecipeBuilder setMaterialCount(MinMaxBounds.Ints count) {
        this.materialCount = count.min().orElse(1);
        return this;
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        return RecipeBuilder.recipeKey(RecipeJson.itemId(result));
    }

    @Override
    protected String type() {
        return "crafting_transmute";
    }

    @Override
    protected void fill(JsonObject json) {
        json.addProperty("category", RecipeJson.craftingCategory(category));
        putGroup(json);
        json.add("input", RecipeJson.ingredient(input));
        json.add("material", RecipeJson.ingredient(material));
        json.add("result", RecipeJson.result(result, resultCount));
    }
}

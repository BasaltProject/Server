// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.CookingBookCategory;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.ItemLike;

public class SimpleCookingRecipeBuilder extends JsonRecipeBuilder {
    private final String kind;
    private final Ingredient ingredient;
    private final CookingBookCategory cookingCategory;
    private final ItemLike result;
    private final float experience;
    private final int cookingTime;

    private SimpleCookingRecipeBuilder(String kind, Ingredient ingredient, RecipeCategory category,
            CookingBookCategory cookingCategory, ItemLike result, float experience, int cookingTime) {
        super(category);
        this.kind = kind;
        this.ingredient = ingredient;
        this.cookingCategory = cookingCategory;
        this.result = result;
        this.experience = experience;
        this.cookingTime = cookingTime;
    }

    public static SimpleCookingRecipeBuilder generic(Ingredient ingredient, RecipeCategory craftingCategory,
            CookingBookCategory cookingCategory, ItemLike result, float experience, int cookingTime,
            net.minecraft.world.item.crafting.AbstractCookingRecipe.Factory<?> factory) {
        return new SimpleCookingRecipeBuilder("smelting", ingredient, craftingCategory, cookingCategory, result, experience, cookingTime);
    }

    public static SimpleCookingRecipeBuilder campfireCooking(Ingredient ingredient, RecipeCategory craftingCategory,
            ItemLike result, float experience, int cookingTime) {
        return new SimpleCookingRecipeBuilder("campfire_cooking", ingredient, craftingCategory, CookingBookCategory.FOOD, result, experience, cookingTime);
    }

    public static SimpleCookingRecipeBuilder blasting(Ingredient ingredient, RecipeCategory craftingCategory,
            CookingBookCategory cookingCategory, ItemLike result, float experience, int cookingTime) {
        return new SimpleCookingRecipeBuilder("blasting", ingredient, craftingCategory, cookingCategory, result, experience, cookingTime);
    }

    public static SimpleCookingRecipeBuilder smelting(Ingredient ingredient, RecipeCategory craftingCategory,
            CookingBookCategory cookingCategory, ItemLike result, float experience, int cookingTime) {
        return new SimpleCookingRecipeBuilder("smelting", ingredient, craftingCategory, cookingCategory, result, experience, cookingTime);
    }

    public static SimpleCookingRecipeBuilder smoking(Ingredient ingredient, RecipeCategory craftingCategory,
            ItemLike result, float experience, int cookingTime) {
        return new SimpleCookingRecipeBuilder("smoking", ingredient, craftingCategory, CookingBookCategory.FOOD, result, experience, cookingTime);
    }

    @Override
    public SimpleCookingRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public SimpleCookingRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        return RecipeBuilder.recipeKey(RecipeJson.itemId(result));
    }

    @Override
    protected String type() {
        return kind;
    }

    @Override
    protected void fill(JsonObject json) {
        json.addProperty("category", cookingCategory.getSerializedName());
        json.addProperty("cookingtime", cookingTime);
        json.addProperty("experience", experience);
        putGroup(json);
        json.add("ingredient", RecipeJson.ingredient(ingredient));
        json.add("result", RecipeJson.result(result, 1));
    }
}

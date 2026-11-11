// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.ItemLike;

public class ShapelessRecipeBuilder extends JsonRecipeBuilder {
    private final Item result;
    private final int count;
    private final JsonArray ingredients = new JsonArray();

    private ShapelessRecipeBuilder(RecipeCategory category, Item result, int count) {
        super(category);
        this.result = result;
        this.count = count;
    }

    public static ShapelessRecipeBuilder shapeless(HolderGetter<Item> items, RecipeCategory category, ItemStackTemplate result) {
        return new ShapelessRecipeBuilder(category, result.item().value(), result.count());
    }

    public static ShapelessRecipeBuilder shapeless(HolderGetter<Item> items, RecipeCategory category, ItemLike item) {
        return shapeless(items, category, item, 1);
    }

    public static ShapelessRecipeBuilder shapeless(HolderGetter<Item> items, RecipeCategory category, ItemLike item, int count) {
        return new ShapelessRecipeBuilder(category, item.asItem(), count);
    }

    public ShapelessRecipeBuilder requires(TagKey<Item> tag) {
        return add(RecipeJson.tagIngredient(tag), 1);
    }

    public ShapelessRecipeBuilder requires(ItemLike item) {
        return requires(item, 1);
    }

    public ShapelessRecipeBuilder requires(ItemLike item, int count) {
        return add(RecipeJson.ingredient(item), count);
    }

    public ShapelessRecipeBuilder requires(Ingredient ingredient) {
        return requires(ingredient, 1);
    }

    public ShapelessRecipeBuilder requires(Ingredient ingredient, int count) {
        return add(RecipeJson.ingredient(ingredient), count);
    }

    private ShapelessRecipeBuilder add(JsonElement value, int count) {
        for (int i = 0; i < count; i++) {
            ingredients.add(value);
        }
        return this;
    }

    @Override
    public ShapelessRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public ShapelessRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        return RecipeBuilder.recipeKey(RecipeJson.itemId(result));
    }

    @Override
    protected String type() {
        return "crafting_shapeless";
    }

    @Override
    protected void fill(JsonObject json) {
        json.addProperty("category", RecipeJson.craftingCategory(category));
        putGroup(json);
        json.add("ingredients", ingredients);
        json.add("result", RecipeJson.result(result, count));
    }
}

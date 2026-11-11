// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.ItemLike;

public class ShapedRecipeBuilder extends JsonRecipeBuilder {
    private final Item result;
    private final int count;
    private final Map<Character, com.google.gson.JsonElement> key = new LinkedHashMap<>();
    private final JsonArray rows = new JsonArray();
    private boolean notify = true;

    private ShapedRecipeBuilder(HolderGetter<Item> items, RecipeCategory category, ItemStackTemplate result) {
        super(category);
        this.result = result.item().value();
        this.count = result.count();
    }

    private ShapedRecipeBuilder(HolderGetter<Item> items, RecipeCategory category, ItemLike result, int count) {
        super(category);
        this.result = result.asItem();
        this.count = count;
    }

    public static ShapedRecipeBuilder shaped(HolderGetter<Item> items, RecipeCategory category, ItemLike item) {
        return shaped(items, category, item, 1);
    }

    public static ShapedRecipeBuilder shaped(HolderGetter<Item> items, RecipeCategory category, ItemLike item, int count) {
        return new ShapedRecipeBuilder(items, category, item, count);
    }

    public ShapedRecipeBuilder define(Character symbol, TagKey<Item> tag) {
        return define(symbol, RecipeJson.tagIngredient(tag));
    }

    public ShapedRecipeBuilder define(Character symbol, ItemLike item) {
        return define(symbol, RecipeJson.ingredient(item));
    }

    public ShapedRecipeBuilder define(Character symbol, Ingredient ingredient) {
        return define(symbol, RecipeJson.ingredient(ingredient));
    }

    private ShapedRecipeBuilder define(Character symbol, com.google.gson.JsonElement value) {
        if (symbol == ' ') {
            throw new IllegalArgumentException("Symbol ' ' (whitespace) is reserved and cannot be defined");
        }
        if (key.put(symbol, value) != null) {
            throw new IllegalArgumentException("Symbol '" + symbol + "' is already defined!");
        }
        return this;
    }

    public ShapedRecipeBuilder pattern(String row) {
        if (rows.size() > 0 && rows.get(0).getAsString().length() != row.length()) {
            throw new IllegalArgumentException("Pattern must be the same width on every line!");
        }
        rows.add(row);
        return this;
    }

    @Override
    public ShapedRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public ShapedRecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    public ShapedRecipeBuilder showNotification(boolean showNotification) {
        this.notify = showNotification;
        return this;
    }

    @Override
    public ResourceKey<Recipe<?>> defaultId() {
        return RecipeBuilder.recipeKey(RecipeJson.itemId(result));
    }

    @Override
    protected String type() {
        return "crafting_shaped";
    }

    @Override
    protected void fill(JsonObject json) {
        json.addProperty("category", RecipeJson.craftingCategory(category));
        putGroup(json);
        JsonObject keys = new JsonObject();
        key.forEach((symbol, value) -> keys.add(String.valueOf(symbol), value));
        json.add("key", keys);
        json.add("pattern", rows);
        json.add("result", RecipeJson.result(result, count));
        if (!notify) {
            json.addProperty("show_notification", false);
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;

public class SmithingTransformRecipeBuilder {
    private final Ingredient template;
    private final Ingredient base;
    private final Ingredient addition;
    private final ItemStackTemplate result;

    public SmithingTransformRecipeBuilder(Ingredient template, Ingredient base, Ingredient addition,
            RecipeCategory category, ItemStackTemplate result) {
        this.template = template;
        this.base = base;
        this.addition = addition;
        this.result = result;
    }

    public static SmithingTransformRecipeBuilder smithing(Ingredient template, Ingredient base, Ingredient addition,
            RecipeCategory category, Item result) {
        return new SmithingTransformRecipeBuilder(template, base, addition, category, new ItemStackTemplate(result));
    }

    public SmithingTransformRecipeBuilder unlocks(String name, Criterion<?> criterion) {
        return this;
    }

    public void save(RecipeOutput output, String id) {
        save(output, RecipeBuilder.recipeKey(Identifier.parse(id)));
    }

    public void save(RecipeOutput output, ResourceKey<Recipe<?>> id) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:smithing_transform");
        json.add("template", RecipeJson.ingredient(template));
        json.add("base", RecipeJson.ingredient(base));
        json.add("addition", RecipeJson.ingredient(addition));
        json.add("result", RecipeJson.result(result.item().value(), result.count()));
        RecipeJson.emit(output, id.identifier(), json);
    }
}

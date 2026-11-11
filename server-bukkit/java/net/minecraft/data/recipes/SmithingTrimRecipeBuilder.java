// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.equipment.trim.TrimPattern;

public class SmithingTrimRecipeBuilder {
    private final Ingredient template;
    private final Ingredient base;
    private final Ingredient addition;
    private final Holder<TrimPattern> pattern;

    public SmithingTrimRecipeBuilder(Ingredient template, Ingredient base, Ingredient addition,
            Holder<TrimPattern> pattern, RecipeCategory category) {
        this.template = template;
        this.base = base;
        this.addition = addition;
        this.pattern = pattern;
    }

    public static SmithingTrimRecipeBuilder smithingTrim(Ingredient template, Ingredient base, Ingredient addition,
            Holder<TrimPattern> pattern, RecipeCategory category) {
        return new SmithingTrimRecipeBuilder(template, base, addition, pattern, category);
    }

    public SmithingTrimRecipeBuilder unlocks(String name, Criterion<?> criterion) {
        return this;
    }

    public void save(RecipeOutput output, ResourceKey<Recipe<?>> id) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:smithing_trim");
        json.add("template", RecipeJson.ingredient(template));
        json.add("base", RecipeJson.ingredient(base));
        json.add("addition", RecipeJson.ingredient(addition));
        pattern.unwrapKey().ifPresent(key -> json.addProperty("pattern", key.identifier().toString()));
        RecipeJson.emit(output, id.identifier(), json);
    }
}

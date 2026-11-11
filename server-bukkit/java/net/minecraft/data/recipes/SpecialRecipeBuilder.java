// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import java.util.function.Supplier;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.Recipe;

public class SpecialRecipeBuilder {
    public SpecialRecipeBuilder(Supplier<Recipe<?>> factory) {
    }

    public static SpecialRecipeBuilder special(Supplier<Recipe<?>> factory) {
        return new SpecialRecipeBuilder(factory);
    }

    public SpecialRecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    public void save(RecipeOutput output, String name) {
        save(output, RecipeBuilder.recipeKey(Identifier.withDefaultNamespace(name)));
    }

    public void save(RecipeOutput output, ResourceKey<Recipe<?>> id) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:crafting_special_" + id.identifier().getPath());
        RecipeJson.emit(output, id.identifier(), json);
    }
}

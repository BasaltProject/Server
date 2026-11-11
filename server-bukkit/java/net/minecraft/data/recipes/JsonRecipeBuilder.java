// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.Recipe;

abstract class JsonRecipeBuilder implements RecipeBuilder {
    protected String groupName = "";
    protected final RecipeCategory category;

    JsonRecipeBuilder(RecipeCategory category) {
        this.category = category;
    }

    protected abstract String type();

    protected abstract void fill(JsonObject json);

    @Override
    public RecipeBuilder unlockedBy(String name, Criterion<?> criterion) {
        return this;
    }

    @Override
    public RecipeBuilder group(String group) {
        this.groupName = group;
        return this;
    }

    @Override
    public void save(RecipeOutput output, ResourceKey<Recipe<?>> id) {
        JsonObject json = new JsonObject();
        json.addProperty("type", "minecraft:" + type());
        fill(json);
        RecipeJson.emit(output, id.identifier(), json);
    }

    protected void putGroup(JsonObject json) {
        if (!groupName.isEmpty()) {
            json.addProperty("group", groupName);
        }
    }
}

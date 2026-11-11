// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.Recipe;

public class RecipeUnlockAdvancementBuilder {
    private final java.util.Map<String, Criterion<?>> criteria = new java.util.LinkedHashMap<>();

    public void unlockedBy(String name, Criterion<?> criterion) {
        criteria.put(name, criterion);
    }

    public AdvancementHolder build(RecipeOutput output, ResourceKey<Recipe<?>> id, RecipeCategory category) {
        return null;
    }
}

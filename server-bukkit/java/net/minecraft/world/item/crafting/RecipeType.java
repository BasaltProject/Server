// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
public interface RecipeType<T extends Recipe<?>> {
    RecipeType<CraftingRecipe> CRAFTING = register("crafting");
    RecipeType<SmeltingRecipe> SMELTING = register("smelting");
    RecipeType<BlastingRecipe> BLASTING = register("blasting");
    RecipeType<SmokingRecipe> SMOKING = register("smoking");
    RecipeType<CampfireCookingRecipe> CAMPFIRE_COOKING = register("campfire_cooking");
    RecipeType<StonecutterRecipe> STONECUTTING = register("stonecutting");
    RecipeType<SmithingRecipe> SMITHING = register("smithing");
    RecipeType<BrewingRecipe> BREWING = register("brewing");
    static <T extends Recipe<?>> RecipeType<T> register(String name) {
        return new RecipeType<T>() {
            @Override
            public String toString() {
                return name;
            }
        };
    }
}

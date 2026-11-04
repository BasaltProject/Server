// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.core.Registry;
public class RecipeBookCategories {
    public static final RecipeBookCategory CRAFTING_BUILDING_BLOCKS = new RecipeBookCategory();
    public static final RecipeBookCategory CRAFTING_REDSTONE = new RecipeBookCategory();
    public static final RecipeBookCategory CRAFTING_EQUIPMENT = new RecipeBookCategory();
    public static final RecipeBookCategory CRAFTING_MISC = new RecipeBookCategory();
    public static final RecipeBookCategory FURNACE_FOOD = new RecipeBookCategory();
    public static final RecipeBookCategory FURNACE_BLOCKS = new RecipeBookCategory();
    public static final RecipeBookCategory FURNACE_MISC = new RecipeBookCategory();
    public static final RecipeBookCategory BLAST_FURNACE_BLOCKS = new RecipeBookCategory();
    public static final RecipeBookCategory BLAST_FURNACE_MISC = new RecipeBookCategory();
    public static final RecipeBookCategory SMOKER_FOOD = new RecipeBookCategory();
    public static final RecipeBookCategory STONECUTTER = new RecipeBookCategory();
    public static final RecipeBookCategory SMITHING = new RecipeBookCategory();
    public static final RecipeBookCategory CAMPFIRE = new RecipeBookCategory();
    public static RecipeBookCategory bootstrap(Registry<RecipeBookCategory> registry) {
        Registry.register(registry, "crafting_building_blocks", CRAFTING_BUILDING_BLOCKS);
Registry.register(registry, "crafting_redstone", CRAFTING_REDSTONE);
Registry.register(registry, "crafting_equipment", CRAFTING_EQUIPMENT);
Registry.register(registry, "crafting_misc", CRAFTING_MISC);
Registry.register(registry, "furnace_food", FURNACE_FOOD);
Registry.register(registry, "furnace_blocks", FURNACE_BLOCKS);
Registry.register(registry, "furnace_misc", FURNACE_MISC);
Registry.register(registry, "blast_furnace_blocks", BLAST_FURNACE_BLOCKS);
Registry.register(registry, "blast_furnace_misc", BLAST_FURNACE_MISC);
Registry.register(registry, "smoker_food", SMOKER_FOOD);
Registry.register(registry, "stonecutter", STONECUTTER);
Registry.register(registry, "smithing", SMITHING);
Registry.register(registry, "campfire", CAMPFIRE);
        return CRAFTING_BUILDING_BLOCKS;
    }
}

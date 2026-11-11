// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.BlockFamilies;
import net.minecraft.data.BlockFamily;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.ItemTags;
import net.minecraft.tags.TagKey;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.crafting.AbstractCookingRecipe;
import net.minecraft.world.item.crafting.CookingBookCategory;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.equipment.trim.TrimPattern;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;

public abstract class RecipeProvider extends RecipeFamilies {
    protected final BootstrapContext<Advancement> advancementOutput;

    protected RecipeProvider(BootstrapContext<Recipe<?>> recipeOutput, BootstrapContext<Advancement> advancementOutput) {
        this.advancementOutput = advancementOutput;
        this.output = new CollectingOutput(recipeOutput, generated);
    }

    protected abstract void buildRecipes();

    public Map<Identifier, JsonObject> generatedRecipes() {
        return generated;
    }

    protected void generateForEnabledBlockFamilies(FeatureFlagSet flagSet) {
        BlockFamilies.getAllFamilies().forEach(family -> generateRecipes(family, flagSet));
    }

    protected void oneToOneConversionRecipe(ItemLike product, ItemLike resource, String group) {
        oneToOneConversionRecipe(product, resource, group, 1);
    }

    protected void oneToOneConversionRecipe(ItemLike product, ItemLike resource, String group, int productCount) {
        shapeless(RecipeCategory.MISC, product, productCount).requires(resource).group(group)
            .unlockedBy(getHasName(resource), has(resource)).save(output, getConversionRecipeName(product, resource));
    }

    protected void oreSmelting(List<ItemLike> smeltables, RecipeCategory craftingCategory, CookingBookCategory cookingCategory,
            ItemLike result, float experience, int cookingTime, String group) {
        for (ItemLike item : smeltables) {
            SimpleCookingRecipeBuilder.smelting(Ingredient.of(item), craftingCategory, cookingCategory, result, experience, cookingTime)
                .group(group).unlockedBy(getHasName(item), has(item)).save(output, getItemName(result) + "_from_smelting_" + getItemName(item));
        }
    }

    protected void oreBlasting(List<ItemLike> smeltables, RecipeCategory craftingCategory, CookingBookCategory cookingCategory,
            ItemLike result, float experience, int cookingTime, String group) {
        for (ItemLike item : smeltables) {
            SimpleCookingRecipeBuilder.blasting(Ingredient.of(item), craftingCategory, cookingCategory, result, experience, cookingTime)
                .group(group).unlockedBy(getHasName(item), has(item)).save(output, getItemName(result) + "_from_blasting_" + getItemName(item));
        }
    }

    protected void netheriteSmithing(Item base, RecipeCategory category, Item result) {
        SmithingTransformRecipeBuilder.smithing(Ingredient.of(Items.NETHERITE_UPGRADE_SMITHING_TEMPLATE), Ingredient.of(base),
            tag(ItemTags.NETHERITE_TOOL_MATERIALS), category, result).unlocks("has_netherite_ingot", has(ItemTags.NETHERITE_TOOL_MATERIALS))
            .save(output, getItemName(result) + "_smithing");
    }

    protected void trimSmithing(Item trimTemplate, ResourceKey<TrimPattern> patternId, ResourceKey<Recipe<?>> id) {
        Holder<TrimPattern> pattern = Holder.Reference.createStandAlone(new HolderOwner<TrimPattern>() {
        }, patternId);
        SmithingTrimRecipeBuilder.smithingTrim(Ingredient.of(trimTemplate), tag(ItemTags.TRIMMABLE_ARMOR), tag(ItemTags.TRIM_MATERIALS),
            pattern, RecipeCategory.MISC).unlocks("has_smithing_trim_template", has(trimTemplate)).save(output, id);
    }

    protected void twoByTwoPacker(RecipeCategory category, ItemLike result, ItemLike ingredient) {
        shaped(category, result, 1).define('#', ingredient).pattern("##").pattern("##").unlockedBy(getHasName(ingredient), has(ingredient)).save(output);
    }

    protected void threeByThreePacker(RecipeCategory category, ItemLike result, ItemLike ingredient, String unlockedBy) {
        shapeless(category, result).requires(ingredient, 9).unlockedBy(unlockedBy, has(ingredient)).save(output);
    }

    protected void threeByThreePacker(RecipeCategory category, ItemLike result, ItemLike ingredient) {
        threeByThreePacker(category, result, ingredient, getHasName(ingredient));
    }

    protected void planksFromLog(ItemLike result, TagKey<Item> logs, int count) {
        shapeless(RecipeCategory.BUILDING_BLOCKS, result, count).requires(logs).group("planks").unlockedBy("has_log", has(logs)).save(output);
    }

    protected void planksFromLogs(ItemLike result, TagKey<Item> logs, int count) {
        shapeless(RecipeCategory.BUILDING_BLOCKS, result, count).requires(logs).group("planks").unlockedBy("has_logs", has(logs)).save(output);
    }

    protected void woodFromLogs(ItemLike result, ItemLike log) {
        shaped(RecipeCategory.BUILDING_BLOCKS, result, 3).define('#', log).pattern("##").pattern("##").group("bark")
            .unlockedBy("has_log", has(log)).save(output);
    }

    protected void woodenBoat(ItemLike result, ItemLike planks) {
        shaped(RecipeCategory.TRANSPORTATION, result).define('#', planks).pattern("# #").pattern("###").group("boat")
            .unlockedBy("in_water", null).save(output);
    }

    protected void chestBoat(ItemLike chestBoat, ItemLike boat) {
        shapeless(RecipeCategory.TRANSPORTATION, chestBoat).requires(Blocks.CHEST).requires(boat).group("chest_boat")
            .unlockedBy("has_boat", has(ItemTags.BOATS)).save(output);
    }

    protected RecipeBuilder doorBuilder(ItemLike result, Ingredient base) {
        return shaped(RecipeCategory.REDSTONE, result, 3).define('#', base).pattern("##").pattern("##").pattern("##");
    }

    protected void pressurePlate(ItemLike result, ItemLike base) {
        pressurePlateBuilder(RecipeCategory.REDSTONE, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void slab(RecipeCategory category, ItemLike result, ItemLike base) {
        slabBuilder(category, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void shelf(ItemLike result, ItemLike strippedLogs) {
        shaped(RecipeCategory.DECORATIONS, result, 6).define('#', strippedLogs).pattern("###").pattern("   ").pattern("###").group("shelf")
            .unlockedBy(getHasName(strippedLogs), has(strippedLogs)).save(output);
    }

    protected RecipeBuilder slabBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 6).define('#', base).pattern("###");
    }

    protected RecipeBuilder stairBuilder(ItemLike result, Ingredient base) {
        return shaped(RecipeCategory.BUILDING_BLOCKS, result, 4).define('#', base).pattern("#  ").pattern("## ").pattern("###");
    }

    protected RecipeBuilder trapdoorBuilder(ItemLike result, Ingredient base) {
        return shaped(RecipeCategory.REDSTONE, result, 2).define('#', base).pattern("###").pattern("###");
    }

    protected RecipeBuilder hangingSignBuilder(ItemLike result, Ingredient ingredient) {
        return shaped(RecipeCategory.DECORATIONS, result, 6).group("hanging_sign").define('#', ingredient).define('X', Items.IRON_CHAIN)
            .pattern("X X").pattern("###").pattern("###");
    }

    protected void stonecutterResultFromBase(RecipeCategory category, ItemLike result, ItemLike base) {
        stonecutterResultFromBase(category, result, base, 1);
    }

    protected void stonecutterResultFromBase(RecipeCategory category, ItemLike result, ItemLike base, int count) {
        SingleItemRecipeBuilder.stonecutting(Ingredient.of(base), category, result, count).unlockedBy(getHasName(base), has(base))
            .save(output, getConversionRecipeName(result, base) + "_stonecutting");
    }

    protected void nineBlockStorageRecipes(RecipeCategory unpackedFormCategory, ItemLike unpackedForm, RecipeCategory packedFormCategory, ItemLike packedForm) {
        nineBlock(unpackedFormCategory, unpackedForm, packedFormCategory, packedForm, getSimpleRecipeName(packedForm), null, getSimpleRecipeName(unpackedForm), null);
    }

    protected void nineBlockStorageRecipesWithCustomPacking(RecipeCategory unpackedFormCategory, ItemLike unpackedForm,
            RecipeCategory packedFormCategory, ItemLike packedForm, String packingRecipeId, String packingRecipeGroup) {
        nineBlock(unpackedFormCategory, unpackedForm, packedFormCategory, packedForm, packingRecipeId, packingRecipeGroup, getSimpleRecipeName(unpackedForm), null);
    }

    protected void nineBlockStorageRecipesRecipesWithCustomUnpacking(RecipeCategory unpackedFormCategory, ItemLike unpackedForm,
            RecipeCategory packedFormCategory, ItemLike packedForm, String unpackingRecipeId, String unpackingRecipeGroup) {
        nineBlock(unpackedFormCategory, unpackedForm, packedFormCategory, packedForm, getSimpleRecipeName(packedForm), null, unpackingRecipeId, unpackingRecipeGroup);
    }

    private void nineBlock(RecipeCategory unpackedCategory, ItemLike unpacked, RecipeCategory packedCategory, ItemLike packed,
            String packId, String packGroup, String unpackId, String unpackGroup) {
        shapeless(unpackedCategory, unpacked, 9).requires(packed).group(unpackGroup).unlockedBy(getHasName(packed), has(packed))
            .save(output, RecipeBuilder.recipeKey(Identifier.parse(unpackId)));
        shaped(packedCategory, packed).define('#', unpacked).pattern("###").pattern("###").pattern("###").group(packGroup)
            .unlockedBy(getHasName(unpacked), has(unpacked)).save(output, RecipeBuilder.recipeKey(Identifier.parse(packId)));
    }

    protected void copySmithingTemplate(ItemLike smithingTemplate, ItemLike baseMaterial) {
        copySmithingTemplate(smithingTemplate, Ingredient.of(baseMaterial));
    }

    protected void copySmithingTemplate(ItemLike smithingTemplate, Ingredient baseMaterials) {
        shaped(RecipeCategory.MISC, smithingTemplate, 2).define('#', Items.DIAMOND).define('C', baseMaterials).define('S', smithingTemplate)
            .pattern("#S#").pattern("#C#").pattern("###").unlockedBy(getHasName(smithingTemplate), has(smithingTemplate)).save(output);
    }

    protected <T extends AbstractCookingRecipe> void cookRecipes(String source, AbstractCookingRecipe.Factory<T> factory, int cookingTime) {
        Object[][] foods = {{Items.BEEF, Items.COOKED_BEEF, 0.35F}, {Items.CHICKEN, Items.COOKED_CHICKEN, 0.35F},
            {Items.COD, Items.COOKED_COD, 0.35F}, {Items.KELP, Items.DRIED_KELP, 0.1F}, {Items.SALMON, Items.COOKED_SALMON, 0.35F},
            {Items.MUTTON, Items.COOKED_MUTTON, 0.35F}, {Items.PORKCHOP, Items.COOKED_PORKCHOP, 0.35F},
            {Items.POTATO, Items.BAKED_POTATO, 0.35F}, {Items.RABBIT, Items.COOKED_RABBIT, 0.35F}};
        for (Object[] food : foods) {
            ItemLike base = (ItemLike) food[0];
            ItemLike result = (ItemLike) food[1];
            Ingredient in = Ingredient.of(base);
            float xp = (Float) food[2];
            SimpleCookingRecipeBuilder builder = switch (source) {
                case "campfire_cooking" -> SimpleCookingRecipeBuilder.campfireCooking(in, RecipeCategory.FOOD, result, xp, cookingTime);
                case "smoking" -> SimpleCookingRecipeBuilder.smoking(in, RecipeCategory.FOOD, result, xp, cookingTime);
                default -> SimpleCookingRecipeBuilder.smelting(in, RecipeCategory.FOOD, CookingBookCategory.FOOD, result, xp, cookingTime);
            };
            builder.unlockedBy(getHasName(base), has(base)).save(output, getItemName(result) + "_from_" + source);
        }
    }

    protected void carpet(ItemLike result, ItemLike sourceItem) {
        shaped(RecipeCategory.DECORATIONS, result, 3).define('#', sourceItem).pattern("##").group("carpet")
            .unlockedBy(getHasName(sourceItem), has(sourceItem)).save(output);
    }

    protected void bedFromPlanksAndWool(ItemLike result, ItemLike wool) {
        shaped(RecipeCategory.DECORATIONS, result).define('#', wool).define('X', ItemTags.PLANKS).pattern("###").pattern("XXX").group("bed")
            .unlockedBy(getHasName(wool), has(wool)).save(output);
    }

    protected void banner(ItemLike result, ItemLike wool) {
        shaped(RecipeCategory.DECORATIONS, result).define('#', wool).define('|', Items.STICK).pattern("###").pattern("###").pattern(" | ")
            .group("banner").unlockedBy(getHasName(wool), has(wool)).save(output);
        SpecialRecipeBuilder.special(() -> null).save(output, getItemName(result) + "_duplicate");
    }

    protected void stainedGlassFromGlassAndDye(ItemLike result, ItemLike dye) {
        shaped(RecipeCategory.BUILDING_BLOCKS, result, 8).define('#', Blocks.GLASS).define('X', dye).pattern("###").pattern("#X#").pattern("###")
            .group("stained_glass").unlockedBy("has_glass", has(Blocks.GLASS)).save(output);
    }

    protected void dryGhast(ItemLike result) {
        shaped(RecipeCategory.BUILDING_BLOCKS, result, 1).define('#', Items.GHAST_TEAR).define('X', Items.SOUL_SAND).pattern("###").pattern("#X#")
            .pattern("###").group("dry_ghast").unlockedBy(getHasName(Items.GHAST_TEAR), has(Items.GHAST_TEAR)).save(output);
    }

    protected void harness(ItemLike result, ItemLike wool) {
        shaped(RecipeCategory.COMBAT, result).define('#', wool).define('G', Items.GLASS).define('L', Items.LEATHER).pattern("LLL").pattern("G#G")
            .group("harness").unlockedBy("has_dried_ghast", has(Blocks.DRIED_GHAST)).save(output);
    }

    protected void stainedGlassPaneFromStainedGlass(ItemLike result, ItemLike stainedGlass) {
        shaped(RecipeCategory.DECORATIONS, result, 16).define('#', stainedGlass).pattern("###").pattern("###").group("stained_glass_pane")
            .unlockedBy("has_glass", has(stainedGlass)).save(output);
    }

    protected void stainedGlassPaneFromGlassPaneAndDye(ItemLike result, ItemLike dye) {
        shaped(RecipeCategory.DECORATIONS, result, 8).define('#', Blocks.GLASS_PANE).define('$', dye).pattern("###").pattern("#$#").pattern("###")
            .group("stained_glass_pane").unlockedBy("has_glass_pane", has(Blocks.GLASS_PANE)).unlockedBy(getHasName(dye), has(dye))
            .save(output, getConversionRecipeName(result, Blocks.GLASS_PANE));
    }

    protected void coloredTerracottaFromTerracottaAndDye(ItemLike result, ItemLike dye) {
        shaped(RecipeCategory.BUILDING_BLOCKS, result, 8).define('#', Blocks.TERRACOTTA).define('X', dye).pattern("###").pattern("#X#").pattern("###")
            .group("stained_terracotta").unlockedBy("has_terracotta", has(Blocks.TERRACOTTA)).save(output);
    }

    protected void concretePowder(ItemLike result, ItemLike dye) {
        shapeless(RecipeCategory.BUILDING_BLOCKS, result, 8).requires(dye).requires(Blocks.SAND, 4).requires(Blocks.GRAVEL, 4).group("concrete_powder")
            .unlockedBy("has_sand", has(Blocks.SAND)).unlockedBy("has_gravel", has(Blocks.GRAVEL)).save(output);
    }

    protected void candle(ItemLike result, ItemLike dye) {
        shapeless(RecipeCategory.DECORATIONS, result).requires(Blocks.CANDLE).requires(dye).group("dyed_candle")
            .unlockedBy(getHasName(dye), has(dye)).save(output);
    }

    protected void wall(RecipeCategory category, ItemLike result, ItemLike base) {
        wallBuilder(category, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void polished(RecipeCategory category, ItemLike result, ItemLike base) {
        polishedBuilder(category, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void cut(RecipeCategory category, ItemLike result, ItemLike base) {
        cutBuilder(category, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void chiseled(RecipeCategory category, ItemLike result, ItemLike base) {
        chiseledBuilder(category, result, Ingredient.of(base)).unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected void mosaicBuilder(RecipeCategory category, ItemLike result, ItemLike base) {
        shaped(category, result).define('#', base).pattern("#").pattern("#").unlockedBy(getHasName(base), has(base)).save(output);
    }

    protected ShapedRecipeBuilder chiseledBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result).define('#', base).pattern("#").pattern("#");
    }

    protected ShapedRecipeBuilder carpetBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 3).define('#', base).pattern("##").group("carpet");
    }

    protected void grate(Block grateBlock, Block material) {
        shaped(RecipeCategory.BUILDING_BLOCKS, grateBlock, 4).define('M', material).pattern(" M ").pattern("M M").pattern(" M ")
            .group(getItemName(grateBlock)).unlockedBy(getHasName(material), has(material)).save(output);
    }

    protected void copperBulb(Block copperBulb, Block copperMaterial) {
        shaped(RecipeCategory.REDSTONE, copperBulb, 4).define('C', copperMaterial).define('R', Items.REDSTONE).define('B', Items.BLAZE_ROD)
            .pattern(" C ").pattern("CBC").pattern(" R ").unlockedBy(getHasName(copperMaterial), has(copperMaterial)).group(getItemName(copperBulb)).save(output);
    }

    protected void waxedChiseled(Block result, Block material) {
        shaped(RecipeCategory.BUILDING_BLOCKS, result).define('M', material).pattern(" M ").pattern(" M ").group(getItemName(result))
            .unlockedBy(getHasName(material), has(material)).save(output);
    }

    protected void cushionRecipe(Item woolSlab, Item result) {
        shaped(RecipeCategory.DECORATIONS, result, 1).define('#', woolSlab).group("cushion").unlockedBy(getHasName(woolSlab), has(woolSlab))
            .pattern("###").save(output);
    }

    protected void dyedShulkerBoxRecipe(Item dye, Item dyedResult) {
        TransmuteRecipeBuilder.transmute(RecipeCategory.DECORATIONS, tag(ItemTags.SHULKER_BOXES), Ingredient.of(dye), dyedResult)
            .group("shulker_box_dye").unlockedBy("has_shulker_box", has(ItemTags.SHULKER_BOXES)).save(output);
    }

    protected void dyedBundleRecipe(Item dye, Item dyedResult) {
        TransmuteRecipeBuilder.transmute(RecipeCategory.TOOLS, tag(ItemTags.BUNDLES), Ingredient.of(dye), dyedResult)
            .group("bundle_dye").unlockedBy(getHasName(dye), has(dye)).save(output);
    }

    protected Criterion<?> bredAnimal() {
        return null;
    }

    protected Criterion<?> has(ItemLike item) {
        return null;
    }

    protected Criterion<?> has(TagKey<Item> tag) {
        return null;
    }

    protected static String getHasName(ItemLike baseBlock) {
        return "has_" + getItemName(baseBlock);
    }

    protected static String getItemName(ItemLike itemLike) {
        return RecipeJson.itemId(itemLike).getPath();
    }

    protected static String getSimpleRecipeName(ItemLike itemLike) {
        return getItemName(itemLike);
    }

    protected static String getConversionRecipeName(ItemLike product, ItemLike material) {
        return getItemName(product) + "_from_" + getItemName(material);
    }

    protected static String getSmeltingRecipeName(ItemLike product) {
        return getItemName(product) + "_from_smelting";
    }

    protected static String getBlastingRecipeName(ItemLike product) {
        return getItemName(product) + "_from_blasting";
    }

    protected Ingredient tag(TagKey<Item> id) {
        return RecipeJson.taggedIngredient(id);
    }

    protected ShapedRecipeBuilder shaped(RecipeCategory category, ItemLike item) {
        return shaped(category, item, 1);
    }

    protected ShapedRecipeBuilder shaped(RecipeCategory category, ItemLike item, int count) {
        return ShapedRecipeBuilder.shaped(null, category, item, count);
    }

    protected ShapelessRecipeBuilder shapeless(RecipeCategory category, ItemStackTemplate result) {
        return ShapelessRecipeBuilder.shapeless(null, category, result);
    }

    protected ShapelessRecipeBuilder shapeless(RecipeCategory category, ItemLike item) {
        return shapeless(category, item, 1);
    }

    protected ShapelessRecipeBuilder shapeless(RecipeCategory category, ItemLike item, int count) {
        return ShapelessRecipeBuilder.shapeless(null, category, item, count);
    }

    interface FamilyCraftingRecipeProvider {
        RecipeBuilder create(RecipeProvider context, ItemLike result, ItemLike base);
    }

    interface FamilyStonecutterRecipeProvider {
        void create(RecipeProvider context, ItemLike result, ItemLike base);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonObject;
import java.util.Map;
import java.util.TreeMap;
import java.util.stream.Stream;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.Registry;
import net.minecraft.data.BlockFamily;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.crafting.CookingBookCategory;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;

abstract class RecipeFamilies {
    protected final Map<Identifier, JsonObject> generated = new TreeMap<>();
    protected RecipeOutput output;

    protected abstract ShapedRecipeBuilder shaped(RecipeCategory category, ItemLike item, int count);

    protected abstract ShapedRecipeBuilder shaped(RecipeCategory category, ItemLike item);

    protected abstract ShapelessRecipeBuilder shapeless(RecipeCategory category, ItemLike item);

    protected abstract Criterion<?> has(ItemLike item);

    protected RecipeBuilder buttonBuilder(ItemLike result, Ingredient base) {
        return shapeless(RecipeCategory.REDSTONE, result).requires(base);
    }

    protected RecipeBuilder fenceBuilder(ItemLike result, Ingredient base) {
        boolean nether = result == Blocks.NETHER_BRICK_FENCE;
        return shaped(RecipeCategory.DECORATIONS, result, nether ? 6 : 3).define('W', base).define('#', nether ? Items.NETHER_BRICK : Items.STICK)
            .pattern("W#W").pattern("W#W");
    }

    protected RecipeBuilder fenceGateBuilder(ItemLike result, Ingredient planks) {
        return shaped(RecipeCategory.REDSTONE, result).define('#', Items.STICK).define('W', planks).pattern("#W#").pattern("#W#");
    }

    protected RecipeBuilder pressurePlateBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result).define('#', base).pattern("##");
    }

    protected RecipeBuilder signBuilder(ItemLike result, Ingredient planks) {
        return shaped(RecipeCategory.DECORATIONS, result, 3).group("sign").define('#', planks).define('X', Items.STICK)
            .pattern("###").pattern("###").pattern(" X ");
    }

    protected RecipeBuilder wallBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 6).define('#', base).pattern("###").pattern("###");
    }

    protected RecipeBuilder squareBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 4).define('#', base).pattern("##").pattern("##");
    }

    protected RecipeBuilder pillarBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 2).define('#', base).pattern("#").pattern("#");
    }

    protected RecipeBuilder polishedBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 4).define('S', base).pattern("SS").pattern("SS");
    }

    protected ShapedRecipeBuilder cutBuilder(RecipeCategory category, ItemLike result, Ingredient base) {
        return shaped(category, result, 4).define('#', base).pattern("##").pattern("##");
    }

    protected abstract RecipeBuilder slabBuilder(RecipeCategory category, ItemLike result, Ingredient base);

    protected abstract RecipeBuilder stairBuilder(ItemLike result, Ingredient base);

    protected abstract RecipeBuilder trapdoorBuilder(ItemLike result, Ingredient base);

    protected abstract RecipeBuilder doorBuilder(ItemLike result, Ingredient base);

    protected abstract RecipeBuilder hangingSignBuilder(ItemLike result, Ingredient ingredient);

    protected abstract ShapedRecipeBuilder chiseledBuilder(RecipeCategory category, ItemLike result, Ingredient base);

    protected abstract ShapedRecipeBuilder carpetBuilder(RecipeCategory category, ItemLike result, Ingredient base);

    protected abstract void stonecutterResultFromBase(RecipeCategory category, ItemLike result, ItemLike base, int count);

    protected static String hasName(ItemLike item) {
        return "has_" + RecipeJson.itemId(item).getPath();
    }

    private RecipeBuilder familyShape(BlockFamily.Variant variant, ItemLike result, Ingredient base) {
        return switch (variant) {
            case BUTTON -> buttonBuilder(result, base);
            case CHISELED -> chiseledBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            case CARPET -> carpetBuilder(RecipeCategory.DECORATIONS, result, base);
            case CUT -> cutBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            case DOOR -> doorBuilder(result, base);
            case CUSTOM_FENCE, FENCE -> fenceBuilder(result, base);
            case CUSTOM_FENCE_GATE, FENCE_GATE -> fenceGateBuilder(result, base);
            case SIGN -> signBuilder(result, base);
            case CUSTOM_HANGING_SIGN, HANGING_SIGN -> hangingSignBuilder(result, base);
            case SLAB -> slabBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            case STAIRS -> stairBuilder(result, base);
            case PRESSURE_PLATE -> pressurePlateBuilder(RecipeCategory.REDSTONE, result, base);
            case POLISHED -> polishedBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            case TRAPDOOR -> trapdoorBuilder(result, base);
            case WALL -> wallBuilder(RecipeCategory.DECORATIONS, result, base);
            case BRICKS, TILES -> squareBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            case PILLAR -> pillarBuilder(RecipeCategory.BUILDING_BLOCKS, result, base);
            default -> null;
        };
    }

    private void familyStonecut(BlockFamily family, BlockFamily.Variant variant, Block base) {
        switch (variant) {
            case SLAB -> stonecutterResultFromBase(RecipeCategory.BUILDING_BLOCKS, family.get(variant), base, 2);
            case STAIRS, BRICKS, CHISELED, POLISHED, CUT, TILES, PILLAR, COBBLED ->
                stonecutterResultFromBase(RecipeCategory.BUILDING_BLOCKS, family.get(variant), base, 1);
            case WALL -> stonecutterResultFromBase(RecipeCategory.DECORATIONS, family.get(variant), base, 1);
            default -> {
            }
        }
        boolean nests = variant == BlockFamily.Variant.POLISHED || variant == BlockFamily.Variant.CUT || variant == BlockFamily.Variant.BRICKS
            || variant == BlockFamily.Variant.TILES || variant == BlockFamily.Variant.PILLAR || variant == BlockFamily.Variant.COBBLED;
        BlockFamily child = nests ? net.minecraft.data.BlockFamilies.getFamily(family.get(variant)) : null;
        if (child != null) {
            child.getVariants().forEach((childVariant, ignored) -> familyStonecut(child, childVariant, base));
        }
    }

    protected void generateRecipes(BlockFamily family, FeatureFlagSet flagSet) {
        family.getVariants().forEach((variant, result) -> {
            if (!result.requiredFeatures().isSubsetOf(flagSet)) {
                return;
            }
            Block base = baseForCrafting(family, variant);
            if (family.shouldGenerateCraftingRecipe()) {
                RecipeBuilder builder = familyShape(variant, result, Ingredient.of(base));
                if (builder != null) {
                    family.getRecipeGroupPrefix().ifPresent(prefix -> builder.group(variant.getPrefixedRecipeGroup(prefix)));
                    builder.unlockedBy(criterionName(family, variant, base), has(base));
                    builder.save(output);
                }
            }
            if (family.shouldGenerateSmeltingRecipe()) {
                if (variant == BlockFamily.Variant.CRACKED) {
                    smeltFromBase(result, base);
                } else if (variant == BlockFamily.Variant.COBBLED) {
                    smeltFromBase(base, result);
                }
            }
            if (family.shouldGenerateStonecutterRecipe()) {
                familyStonecut(family, variant, family.getBaseBlock());
            }
        });
    }

    private void smeltFromBase(ItemLike result, ItemLike base) {
        SimpleCookingRecipeBuilder.smelting(Ingredient.of(base), RecipeCategory.BUILDING_BLOCKS, CookingBookCategory.BLOCKS, result, 0.1F, 200)
            .unlockedBy(hasName(base), has(base)).save(output);
    }

    private Block baseForCrafting(BlockFamily family, BlockFamily.Variant variant) {
        BlockFamily.Variant baseVariant = variant.getBaseVariantForCrafting();
        if (baseVariant == null) {
            return family.getBaseBlock();
        }
        if (!family.getVariants().containsKey(baseVariant)) {
            throw new IllegalStateException(baseVariant.name() + " is not defined for the family.");
        }
        return family.get(baseVariant);
    }

    private String criterionName(BlockFamily family, BlockFamily.Variant variant, ItemLike base) {
        if (base != family.getBaseBlock()) {
            return "has_" + variant.getBaseVariantForCrafting().getRecipeGroup();
        }
        return family.getRecipeUnlockedBy().orElseGet(() -> hasName(base));
    }

    protected static final class CollectingOutput implements RecipeOutput, JsonRecipeSink {
        private final BootstrapContext<Recipe<?>> context;
        private final Map<Identifier, JsonObject> sink;

        CollectingOutput(BootstrapContext<Recipe<?>> context, Map<Identifier, JsonObject> sink) {
            this.context = context;
            this.sink = sink;
        }

        @Override
        public void acceptJson(Identifier id, JsonObject recipe) {
            sink.put(id, recipe);
        }

        @Override
        public void accept(ResourceKey<Recipe<?>> id, Recipe<?> recipe, AdvancementHolder advancement) {
        }

        @Override
        public Advancement.Builder advancement() {
            return Advancement.Builder.recipeAdvancement().parent(RecipeBuilder.ROOT_RECIPE_ADVANCEMENT);
        }

        @Override
        public <S> HolderGetter<S> lookup(ResourceKey<? extends Registry<? extends S>> key) {
            return context == null ? null : context.lookup(key);
        }

        @Override
        public <S> Stream<net.minecraft.core.Holder.Reference<S>> listContextElements(ResourceKey<? extends Registry<? extends S>> key) {
            return context == null ? Stream.empty() : context.listContextElements(key);
        }
    }
}

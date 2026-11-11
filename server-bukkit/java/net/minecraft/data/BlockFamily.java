// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import com.google.common.collect.Maps;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import net.minecraft.world.level.block.Block;

public class BlockFamily {
    private final Block baseBlock;
    private final Map<Variant, Block> variants = Maps.newHashMap();
    private boolean generateModel = true;
    private boolean generateRecipe = true;
    private boolean generateSmelting = true;
    private boolean generateStonecutter;
    private String recipeGroupPrefix;
    private String recipeUnlockedBy;

    private BlockFamily(Block baseBlock) {
        this.baseBlock = baseBlock;
    }

    public Block getBaseBlock() {
        return baseBlock;
    }

    public Map<Variant, Block> getVariants() {
        return variants;
    }

    public Block get(Variant variant) {
        return variants.get(variant);
    }

    public boolean shouldGenerateModel() {
        return generateModel;
    }

    public boolean shouldGenerateCraftingRecipe() {
        return generateRecipe;
    }

    public boolean shouldGenerateSmeltingRecipe() {
        return generateSmelting;
    }

    public boolean shouldGenerateStonecutterRecipe() {
        return generateStonecutter;
    }

    public Optional<String> getRecipeGroupPrefix() {
        return Optional.ofNullable(recipeGroupPrefix);
    }

    public Optional<String> getRecipeUnlockedBy() {
        return Optional.ofNullable(recipeUnlockedBy);
    }

    public static enum Variant {
        BUTTON("button"), CARPET("carpet"), CHISELED("chiseled", "slab"), CRACKED("cracked"), CUT("cut"), DOOR("door"),
        CUSTOM_FENCE("fence"), FENCE("fence"), CUSTOM_FENCE_GATE("fence_gate"), FENCE_GATE("fence_gate"),
        CUSTOM_HANGING_SIGN("hanging_sign", "stripped_log"), HANGING_SIGN("hanging_sign", "stripped_log"), LOG("log"),
        STRIPPED_LOG("stripped_log"), MOSAIC("mosaic"), SIGN("sign"), SLAB("slab"), STAIRS("stairs"),
        PRESSURE_PLATE("pressure_plate"), POLISHED("polished"), TRAPDOOR("trapdoor"), WALL("wall"), WALL_SIGN("wall_sign"),
        CUSTOM_WALL_HANGING_SIGN("wall_hanging_sign"), WALL_HANGING_SIGN("wall_hanging_sign"), BRICKS("bricks"),
        COBBLED("cobbled"), TILES("tiles"), PILLAR("pillar");

        private final String recipeGroup;
        private final String craftingBase;

        Variant(String recipeGroup) {
            this(recipeGroup, null);
        }

        Variant(String recipeGroup, String craftingBase) {
            this.recipeGroup = recipeGroup;
            this.craftingBase = craftingBase;
        }

        public String getRecipeGroup() {
            return recipeGroup;
        }

        public String getPrefixedRecipeGroup(String prefix) {
            return this == CUT ? prefix : prefix + "_" + recipeGroup;
        }

        public Variant getBaseVariantForCrafting() {
            return craftingBase == null ? null : valueOf(craftingBase.toUpperCase(Locale.ROOT));
        }
    }

    public static class Builder {
        private final BlockFamily family;

        public Builder(Block baseBlock) {
            this.family = new BlockFamily(baseBlock);
        }

        public BlockFamily getFamily() {
            return family;
        }

        private Builder put(Variant variant, Block block) {
            family.variants.put(variant, block);
            return this;
        }

        public Builder button(Block block) {
            return put(Variant.BUTTON, block);
        }

        public Builder carpet(Block block) {
            return put(Variant.CARPET, block);
        }

        public Builder chiseled(Block block) {
            return put(Variant.CHISELED, block);
        }

        public Builder mosaic(Block block) {
            return put(Variant.MOSAIC, block);
        }

        public Builder cracked(Block block) {
            return put(Variant.CRACKED, block);
        }

        public Builder tiles(Block block) {
            return put(Variant.TILES, block);
        }

        public Builder pillar(Block block) {
            return put(Variant.PILLAR, block);
        }

        public Builder cut(Block block) {
            return put(Variant.CUT, block);
        }

        public Builder door(Block block) {
            return put(Variant.DOOR, block);
        }

        public Builder customFence(Block block) {
            return put(Variant.CUSTOM_FENCE, block);
        }

        public Builder fence(Block block) {
            return put(Variant.FENCE, block);
        }

        public Builder customFenceGate(Block block) {
            return put(Variant.CUSTOM_FENCE_GATE, block);
        }

        public Builder fenceGate(Block block) {
            return put(Variant.FENCE_GATE, block);
        }

        public Builder log(Block block) {
            return put(Variant.LOG, block);
        }

        public Builder strippedLog(Block block) {
            return put(Variant.STRIPPED_LOG, block);
        }

        public Builder slab(Block block) {
            return put(Variant.SLAB, block);
        }

        public Builder stairs(Block block) {
            return put(Variant.STAIRS, block);
        }

        public Builder pressurePlate(Block block) {
            return put(Variant.PRESSURE_PLATE, block);
        }

        public Builder polished(Block block) {
            return put(Variant.POLISHED, block);
        }

        public Builder trapdoor(Block block) {
            return put(Variant.TRAPDOOR, block);
        }

        public Builder wall(Block block) {
            return put(Variant.WALL, block);
        }

        public Builder cobbled(Block block) {
            return put(Variant.COBBLED, block);
        }

        public Builder bricks(Block block) {
            return put(Variant.BRICKS, block);
        }

        public Builder sign(Block sign, Block wallSign) {
            return put(Variant.SIGN, sign).put(Variant.WALL_SIGN, wallSign);
        }

        public Builder customHangingSign(Block sign, Block wallSign) {
            return put(Variant.CUSTOM_HANGING_SIGN, sign).put(Variant.CUSTOM_WALL_HANGING_SIGN, wallSign);
        }

        public Builder hangingSign(Block sign, Block wallSign) {
            return put(Variant.HANGING_SIGN, sign).put(Variant.WALL_HANGING_SIGN, wallSign);
        }

        public Builder dontGenerateModel() {
            family.generateModel = false;
            return this;
        }

        public Builder dontGenerateCraftingRecipe() {
            family.generateRecipe = false;
            return this;
        }

        public Builder dontGenerateSmeltingRecipe() {
            family.generateSmelting = false;
            return this;
        }

        public Builder generateStonecutterRecipe() {
            family.generateStonecutter = true;
            return this;
        }

        public Builder recipeGroupPrefix(String recipeGroupPrefix) {
            family.recipeGroupPrefix = recipeGroupPrefix;
            return this;
        }

        public Builder recipeUnlockedBy(String recipeUnlockedBy) {
            family.recipeUnlockedBy = recipeUnlockedBy;
            return this;
        }
    }
}

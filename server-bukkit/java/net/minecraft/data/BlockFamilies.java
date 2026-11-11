// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.stream.Stream;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.ColorCollection;
import net.minecraft.world.level.block.WeatheringCopperCollection;

public class BlockFamilies {
    private static final String[] kStates = {"", "exposed_", "weathered_", "oxidized_"};
    private static final Map<Block, BlockFamily> kFamilies = new HashMap<>();
    private static final String kWoodenPrefix = "wooden";
    private static final String kWoolenPrefix = "woolen";
    private static final String kPlanksUnlock = "has_planks";

    public static final BlockFamily ACACIA_PLANKS = wood("acacia", "log");
    public static final BlockFamily CHERRY_PLANKS = wood("cherry", "log");
    public static final BlockFamily BIRCH_PLANKS = wood("birch", "log");
    public static final BlockFamily CRIMSON_PLANKS = wood("crimson", "stem");
    public static final BlockFamily JUNGLE_PLANKS = wood("jungle", "log");
    public static final BlockFamily OAK_PLANKS = wood("oak", "log");
    public static final BlockFamily DARK_OAK_PLANKS = wood("dark_oak", "log");
    public static final BlockFamily PALE_OAK_PLANKS = wood("pale_oak", "log");
    public static final BlockFamily POPLAR_PLANKS = wood("poplar", "log");
    public static final BlockFamily SPRUCE_PLANKS = wood("spruce", "log");
    public static final BlockFamily WARPED_PLANKS = wood("warped", "stem");
    public static final BlockFamily MANGROVE_PLANKS = wood("mangrove", "log");
    public static final BlockFamily BAMBOO_PLANKS = bambooPlanks();
    public static final BlockFamily BAMBOO_MOSAIC = stone("bamboo_mosaic", "slab:bamboo_mosaic_slab stairs:bamboo_mosaic_stairs !stonecut");
    public static final BlockFamily MUD_BRICKS = stone("mud_bricks", "wall:mud_brick_wall stairs:mud_brick_stairs slab:mud_brick_slab");
    public static final BlockFamily ANDESITE = stone("andesite", "wall:andesite_wall stairs:andesite_stairs slab:andesite_slab polished:polished_andesite");
    public static final BlockFamily POLISHED_ANDESITE = stone("polished_andesite", "stairs:polished_andesite_stairs slab:polished_andesite_slab");
    public static final BlockFamily BLACKSTONE = stone("blackstone", "wall:blackstone_wall stairs:blackstone_stairs slab:blackstone_slab polished:polished_blackstone");
    public static final BlockFamily POLISHED_BLACKSTONE = stone("polished_blackstone", "wall:polished_blackstone_wall pressurePlate:polished_blackstone_pressure_plate "
        + "button:polished_blackstone_button stairs:polished_blackstone_stairs slab:polished_blackstone_slab bricks:polished_blackstone_bricks chiseled:chiseled_polished_blackstone");
    public static final BlockFamily POLISHED_BLACKSTONE_BRICKS = stone("polished_blackstone_bricks", "wall:polished_blackstone_brick_wall stairs:polished_blackstone_brick_stairs "
        + "slab:polished_blackstone_brick_slab cracked:cracked_polished_blackstone_bricks");
    public static final BlockFamily BRICKS = stone("bricks", "wall:brick_wall stairs:brick_stairs slab:brick_slab");
    public static final BlockFamily END_STONE = stone("end_stone", "bricks:end_stone_bricks");
    public static final BlockFamily END_STONE_BRICKS = stone("end_stone_bricks", "wall:end_stone_brick_wall stairs:end_stone_brick_stairs slab:end_stone_brick_slab");
    public static final BlockFamily MOSSY_STONE_BRICKS = stone("mossy_stone_bricks", "wall:mossy_stone_brick_wall stairs:mossy_stone_brick_stairs slab:mossy_stone_brick_slab");
    public static final WeatheringCopperCollection<BlockFamily> CUT_COPPER = copperFamilies(false);
    public static final WeatheringCopperCollection<BlockFamily> COPPER_BLOCK = copperFamilies(true);
    public static final BlockFamily COBBLESTONE = stone("cobblestone", "wall:cobblestone_wall stairs:cobblestone_stairs slab:cobblestone_slab");
    public static final BlockFamily MOSSY_COBBLESTONE = stone("mossy_cobblestone", "wall:mossy_cobblestone_wall stairs:mossy_cobblestone_stairs slab:mossy_cobblestone_slab");
    public static final BlockFamily DIORITE = stone("diorite", "wall:diorite_wall stairs:diorite_stairs slab:diorite_slab polished:polished_diorite");
    public static final BlockFamily POLISHED_DIORITE = stone("polished_diorite", "stairs:polished_diorite_stairs slab:polished_diorite_slab");
    public static final BlockFamily GRANITE = stone("granite", "wall:granite_wall stairs:granite_stairs slab:granite_slab polished:polished_granite");
    public static final BlockFamily SULFUR = stone("sulfur", "wall:sulfur_wall stairs:sulfur_stairs slab:sulfur_slab chiseled:chiseled_sulfur polished:polished_sulfur");
    public static final BlockFamily POLISHED_SULFUR = stone("polished_sulfur", "wall:polished_sulfur_wall stairs:polished_sulfur_stairs slab:polished_sulfur_slab bricks:sulfur_bricks");
    public static final BlockFamily SULFUR_BRICKS = stone("sulfur_bricks", "wall:sulfur_brick_wall stairs:sulfur_brick_stairs slab:sulfur_brick_slab");
    public static final BlockFamily CINNABAR = stone("cinnabar", "wall:cinnabar_wall stairs:cinnabar_stairs slab:cinnabar_slab chiseled:chiseled_cinnabar polished:polished_cinnabar");
    public static final BlockFamily POLISHED_CINNABAR = stone("polished_cinnabar", "wall:polished_cinnabar_wall stairs:polished_cinnabar_stairs slab:polished_cinnabar_slab bricks:cinnabar_bricks");
    public static final BlockFamily CINNABAR_BRICKS = stone("cinnabar_bricks", "wall:cinnabar_brick_wall stairs:cinnabar_brick_stairs slab:cinnabar_brick_slab");
    public static final BlockFamily POLISHED_GRANITE = stone("polished_granite", "stairs:polished_granite_stairs slab:polished_granite_slab");
    public static final BlockFamily TUFF = stone("tuff", "wall:tuff_wall stairs:tuff_stairs slab:tuff_slab chiseled:chiseled_tuff polished:polished_tuff");
    public static final BlockFamily POLISHED_TUFF = stone("polished_tuff", "wall:polished_tuff_wall stairs:polished_tuff_stairs slab:polished_tuff_slab bricks:tuff_bricks");
    public static final BlockFamily TUFF_BRICKS = stone("tuff_bricks", "wall:tuff_brick_wall stairs:tuff_brick_stairs slab:tuff_brick_slab chiseled:chiseled_tuff_bricks");
    public static final BlockFamily RESIN_BRICKS = stone("resin_bricks", "wall:resin_brick_wall stairs:resin_brick_stairs slab:resin_brick_slab chiseled:chiseled_resin_bricks");
    public static final BlockFamily NETHER_BRICKS = stone("nether_bricks", "fence:nether_brick_fence wall:nether_brick_wall stairs:nether_brick_stairs slab:nether_brick_slab "
        + "chiseled:chiseled_nether_bricks cracked:cracked_nether_bricks");
    public static final BlockFamily RED_NETHER_BRICKS = stone("red_nether_bricks", "slab:red_nether_brick_slab stairs:red_nether_brick_stairs wall:red_nether_brick_wall");
    public static final BlockFamily PRISMARINE = stone("prismarine", "wall:prismarine_wall stairs:prismarine_stairs slab:prismarine_slab");
    public static final BlockFamily PRISMARINE_BRICKS = stone("prismarine_bricks", "stairs:prismarine_brick_stairs slab:prismarine_brick_slab");
    public static final BlockFamily DARK_PRISMARINE = stone("dark_prismarine", "stairs:dark_prismarine_stairs slab:dark_prismarine_slab");
    public static final BlockFamily PURPUR = stone("purpur_block", "stairs:purpur_stairs slab:purpur_slab pillar:purpur_pillar !crafting");
    public static final BlockFamily QUARTZ = stone("quartz_block", "stairs:quartz_stairs slab:quartz_slab chiseled:chiseled_quartz_block bricks:quartz_bricks pillar:quartz_pillar !crafting");
    public static final BlockFamily SMOOTH_QUARTZ = stone("smooth_quartz", "stairs:smooth_quartz_stairs slab:smooth_quartz_slab");
    public static final BlockFamily SANDSTONE = stone("sandstone", "wall:sandstone_wall stairs:sandstone_stairs slab:sandstone_slab chiseled:chiseled_sandstone cut:cut_sandstone !crafting");
    public static final BlockFamily CUT_SANDSTONE = stone("cut_sandstone", "slab:cut_sandstone_slab");
    public static final BlockFamily SMOOTH_SANDSTONE = stone("smooth_sandstone", "slab:smooth_sandstone_slab stairs:smooth_sandstone_stairs");
    public static final BlockFamily RED_SANDSTONE = stone("red_sandstone", "wall:red_sandstone_wall stairs:red_sandstone_stairs slab:red_sandstone_slab "
        + "chiseled:chiseled_red_sandstone cut:cut_red_sandstone !crafting");
    public static final BlockFamily CUT_RED_SANDSTONE = stone("cut_red_sandstone", "slab:cut_red_sandstone_slab");
    public static final BlockFamily SMOOTH_RED_SANDSTONE = stone("smooth_red_sandstone", "slab:smooth_red_sandstone_slab stairs:smooth_red_sandstone_stairs");
    public static final BlockFamily STONE = stone("stone", "slab:stone_slab pressurePlate:stone_pressure_plate button:stone_button stairs:stone_stairs bricks:stone_bricks cobbled:cobblestone");
    public static final BlockFamily STONE_BRICK = stone("stone_bricks", "wall:stone_brick_wall stairs:stone_brick_stairs slab:stone_brick_slab chiseled:chiseled_stone_bricks "
        + "cracked:cracked_stone_bricks !crafting");
    public static final BlockFamily DEEPSLATE = stone("deepslate", "cobbled:cobbled_deepslate");
    public static final BlockFamily COBBLED_DEEPSLATE = stone("cobbled_deepslate", "slab:cobbled_deepslate_slab stairs:cobbled_deepslate_stairs wall:cobbled_deepslate_wall "
        + "chiseled:chiseled_deepslate polished:polished_deepslate");
    public static final BlockFamily POLISHED_DEEPSLATE = stone("polished_deepslate", "slab:polished_deepslate_slab stairs:polished_deepslate_stairs wall:polished_deepslate_wall bricks:deepslate_bricks");
    public static final BlockFamily DEEPSLATE_BRICKS = stone("deepslate_bricks", "slab:deepslate_brick_slab stairs:deepslate_brick_stairs wall:deepslate_brick_wall "
        + "cracked:cracked_deepslate_bricks tiles:deepslate_tiles");
    public static final BlockFamily DEEPSLATE_TILES = stone("deepslate_tiles", "slab:deepslate_tile_slab stairs:deepslate_tile_stairs wall:deepslate_tile_wall cracked:cracked_deepslate_tiles");
    public static final ColorCollection<BlockFamily> WOOL = ColorCollection.VALUES.map(color -> colored(color, "wool", true));
    public static final ColorCollection<BlockFamily> CONCRETE = ColorCollection.VALUES.map(color -> colored(color, "concrete", false));

    private static Block block(String name) {
        Block found = BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(name));
        if (found == null) {
            throw new IllegalStateException("Unknown block in a block family: " + name);
        }
        return found;
    }

    private static BlockFamily register(BlockFamily.Builder builder) {
        BlockFamily family = builder.getFamily();
        kFamilies.put(family.getBaseBlock(), family);
        return family;
    }

    private static BlockFamily apply(BlockFamily.Builder builder, String spec) {
        for (String token : spec.split(" ")) {
            if (token.equals("!stonecut")) {
                continue;
            }
            if (token.equals("!crafting")) {
                builder.dontGenerateCraftingRecipe();
                continue;
            }
            int colon = token.indexOf(':');
            Block target = block(token.substring(colon + 1));
            switch (token.substring(0, colon)) {
                case "wall" -> builder.wall(target);
                case "stairs" -> builder.stairs(target);
                case "slab" -> builder.slab(target);
                case "polished" -> builder.polished(target);
                case "bricks" -> builder.bricks(target);
                case "chiseled" -> builder.chiseled(target);
                case "cracked" -> builder.cracked(target);
                case "cut" -> builder.cut(target);
                case "fence" -> builder.fence(target);
                case "pillar" -> builder.pillar(target);
                case "cobbled" -> builder.cobbled(target);
                case "tiles" -> builder.tiles(target);
                case "button" -> builder.button(target);
                case "pressurePlate" -> builder.pressurePlate(target);
                default -> throw new IllegalArgumentException("Unknown family variant in " + token);
            }
        }
        return register(builder);
    }

    private static BlockFamily stone(String base, String spec) {
        BlockFamily.Builder builder = new BlockFamily.Builder(block(base));
        if (!spec.contains("!stonecut")) {
            builder.generateStonecutterRecipe();
        }
        return apply(builder, spec);
    }

    private static BlockFamily wood(String type, String logKind) {
        String log = logKind.equals("stem") ? type + "_stem" : type + "_log";
        String strippedLog = logKind.equals("stem") ? "stripped_" + type + "_stem" : "stripped_" + type + "_log";
        BlockFamily.Builder builder = new BlockFamily.Builder(block(type + "_planks"))
            .log(block(log)).strippedLog(block(strippedLog)).button(block(type + "_button")).fence(block(type + "_fence"))
            .fenceGate(block(type + "_fence_gate")).hangingSign(block(type + "_hanging_sign"), block(type + "_wall_hanging_sign"))
            .pressurePlate(block(type + "_pressure_plate")).sign(block(type + "_sign"), block(type + "_wall_sign"))
            .slab(block(type + "_slab")).stairs(block(type + "_stairs")).door(block(type + "_door")).trapdoor(block(type + "_trapdoor"))
            .recipeGroupPrefix(kWoodenPrefix).recipeUnlockedBy(kPlanksUnlock);
        return register(builder);
    }

    private static BlockFamily bambooPlanks() {
        BlockFamily.Builder builder = new BlockFamily.Builder(block("bamboo_planks"))
            .log(block("bamboo_block")).strippedLog(block("stripped_bamboo_block")).button(block("bamboo_button")).slab(block("bamboo_slab"))
            .stairs(block("bamboo_stairs")).customFence(block("bamboo_fence")).customFenceGate(block("bamboo_fence_gate"))
            .customHangingSign(block("bamboo_hanging_sign"), block("bamboo_wall_hanging_sign")).pressurePlate(block("bamboo_pressure_plate"))
            .sign(block("bamboo_sign"), block("bamboo_wall_sign")).door(block("bamboo_door")).trapdoor(block("bamboo_trapdoor"))
            .recipeGroupPrefix(kWoodenPrefix).recipeUnlockedBy(kPlanksUnlock);
        return register(builder);
    }

    private static BlockFamily colored(DyeColor color, String kind, boolean wool) {
        String name = color.getName() + "_" + kind;
        BlockFamily.Builder builder = new BlockFamily.Builder(block(name)).stairs(block(name + "_stairs")).slab(block(name + "_slab"));
        if (wool) {
            builder.carpet(block(color.getName() + "_carpet")).recipeGroupPrefix(kWoolenPrefix);
        } else {
            builder.recipeGroupPrefix("concrete").generateStonecutterRecipe();
        }
        return register(builder);
    }


    private static BlockFamily copperFamily(boolean cutBase, String waxPrefix, String state, boolean block) {
        String wax = waxPrefix + state;
        String cutName = wax + "cut_copper";
        String baseName = block ? wax + (state.isEmpty() ? "copper_block" : "copper") : cutName;
        BlockFamily.Builder builder = new BlockFamily.Builder(block(baseName)).recipeGroupPrefix(wax + "cut_copper").dontGenerateModel();
        if (block) {
            builder.cut(block(cutName));
        } else {
            builder.slab(block(cutName + "_slab")).stairs(block(cutName + "_stairs")).generateStonecutterRecipe();
        }
        return register(builder);
    }

    private static WeatheringCopperCollection<BlockFamily> copperFamilies(boolean block) {
        BlockFamily[] weathering = new BlockFamily[4];
        BlockFamily[] waxed = new BlockFamily[4];
        for (int i = 0; i < 4; i++) {
            weathering[i] = copperFamily(!block, "", kStates[i], block);
            waxed[i] = copperFamily(!block, "waxed_", kStates[i], block);
        }
        return new WeatheringCopperCollection<>(
            new WeatheringCopperCollection.ByState<>(weathering[0], weathering[1], weathering[2], weathering[3]),
            new WeatheringCopperCollection.ByState<>(waxed[0], waxed[1], waxed[2], waxed[3]));
    }

    public static Stream<BlockFamily> getAllFamilies() {
        return kFamilies.values().stream();
    }

    public static BlockFamily getFamily(Block base) {
        return kFamilies.get(base);
    }
}

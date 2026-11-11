// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import java.util.List;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
public class VegetationPlacements {
    public static final ResourceKey<PlacedFeature> BAMBOO_LIGHT = k("bamboo_light");
    public static final ResourceKey<PlacedFeature> BAMBOO = k("bamboo");
    public static final ResourceKey<PlacedFeature> BAMBOO_IN_STRUCTURE = k("bamboo_in_structure");
    public static final ResourceKey<PlacedFeature> VINES = k("vines");
    public static final ResourceKey<PlacedFeature> PATCH_SUNFLOWER = k("patch_sunflower");
    public static final ResourceKey<PlacedFeature> PATCH_PUMPKIN = k("patch_pumpkin");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_PLAIN = k("patch_grass_plain");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_MEADOW = k("patch_grass_meadow");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_FOREST = k("patch_grass_forest");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_BADLANDS = k("patch_grass_badlands");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_SAVANNA = k("patch_grass_savanna");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_NORMAL = k("patch_grass_normal");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_TAIGA_2 = k("patch_grass_taiga_2");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_TAIGA = k("patch_grass_taiga");
    public static final ResourceKey<PlacedFeature> PATCH_GRASS_JUNGLE = k("patch_grass_jungle");
    public static final ResourceKey<PlacedFeature> GRASS_BONEMEAL = k("grass_bonemeal");
    public static final ResourceKey<PlacedFeature> PATCH_DEAD_BUSH_2 = k("patch_dead_bush_2");
    public static final ResourceKey<PlacedFeature> PATCH_DEAD_BUSH = k("patch_dead_bush");
    public static final ResourceKey<PlacedFeature> PATCH_DEAD_BUSH_BADLANDS = k("patch_dead_bush_badlands");
    public static final ResourceKey<PlacedFeature> PATCH_DRY_GRASS_BADLANDS = k("patch_dry_grass_badlands");
    public static final ResourceKey<PlacedFeature> PATCH_DRY_GRASS_DESERT = k("patch_dry_grass_desert");
    public static final ResourceKey<PlacedFeature> PATCH_MELON = k("patch_melon");
    public static final ResourceKey<PlacedFeature> PATCH_MELON_SPARSE = k("patch_melon_sparse");
    public static final ResourceKey<PlacedFeature> PATCH_BERRY_COMMON = k("patch_berry_common");
    public static final ResourceKey<PlacedFeature> PATCH_BERRY_RARE = k("patch_berry_rare");
    public static final ResourceKey<PlacedFeature> PATCH_WATERLILY = k("patch_waterlily");
    public static final ResourceKey<PlacedFeature> PATCH_TALL_GRASS_2 = k("patch_tall_grass_2");
    public static final ResourceKey<PlacedFeature> PATCH_TALL_GRASS = k("patch_tall_grass");
    public static final ResourceKey<PlacedFeature> PATCH_LARGE_FERN = k("patch_large_fern");
    public static final ResourceKey<PlacedFeature> PATCH_BUSH = k("patch_bush");
    public static final ResourceKey<PlacedFeature> PATCH_RED_SHRUB = k("patch_red_shrub");
    public static final ResourceKey<PlacedFeature> PATCH_LEAF_LITTER = k("patch_leaf_litter");
    public static final ResourceKey<PlacedFeature> PATCH_CACTUS_DESERT = k("patch_cactus_desert");
    public static final ResourceKey<PlacedFeature> PATCH_CACTUS_DECORATED = k("patch_cactus_decorated");
    public static final ResourceKey<PlacedFeature> PATCH_SUGAR_CANE_SWAMP = k("patch_sugar_cane_swamp");
    public static final ResourceKey<PlacedFeature> PATCH_SUGAR_CANE_DESERT = k("patch_sugar_cane_desert");
    public static final ResourceKey<PlacedFeature> PATCH_SUGAR_CANE_BADLANDS = k("patch_sugar_cane_badlands");
    public static final ResourceKey<PlacedFeature> PATCH_SUGAR_CANE = k("patch_sugar_cane");
    public static final ResourceKey<PlacedFeature> PATCH_FIREFLY_BUSH_SWAMP = k("patch_firefly_bush_swamp");
    public static final ResourceKey<PlacedFeature> PATCH_FIREFLY_BUSH_NEAR_WATER_SWAMP = k("patch_firefly_bush_near_water_swamp");
    public static final ResourceKey<PlacedFeature> PATCH_FIREFLY_BUSH_NEAR_WATER = k("patch_firefly_bush_near_water");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_NETHER = k("brown_mushroom_nether");
    public static final ResourceKey<PlacedFeature> RED_MUSHROOM_NETHER = k("red_mushroom_nether");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_NORMAL = k("brown_mushroom_normal");
    public static final ResourceKey<PlacedFeature> RED_MUSHROOM_NORMAL = k("red_mushroom_normal");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_TAIGA = k("brown_mushroom_taiga");
    public static final ResourceKey<PlacedFeature> RED_MUSHROOM_TAIGA = k("red_mushroom_taiga");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_OLD_GROWTH = k("brown_mushroom_old_growth");
    public static final ResourceKey<PlacedFeature> RED_MUSHROOM_OLD_GROWTH = k("red_mushroom_old_growth");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_SWAMP = k("brown_mushroom_swamp");
    public static final ResourceKey<PlacedFeature> RED_MUSHROOM_SWAMP = k("red_mushroom_swamp");
    public static final ResourceKey<PlacedFeature> BROWN_MUSHROOM_DAPPLED_FOREST = k("brown_mushroom_dappled_forest");
    public static final ResourceKey<PlacedFeature> FLOWER_WARM = k("flower_warm");
    public static final ResourceKey<PlacedFeature> FLOWER_DEFAULT = k("flower_default");
    public static final ResourceKey<PlacedFeature> FLOWER_FLOWER_FOREST = k("flower_flower_forest");
    public static final ResourceKey<PlacedFeature> FLOWER_SWAMP = k("flower_swamp");
    public static final ResourceKey<PlacedFeature> FLOWER_PLAINS = k("flower_plains");
    public static final ResourceKey<PlacedFeature> FLOWER_MEADOW = k("flower_meadow");
    public static final ResourceKey<PlacedFeature> FLOWER_CHERRY = k("flower_cherry");
    public static final ResourceKey<PlacedFeature> FLOWER_PALE_GARDEN = k("flower_pale_garden");
    public static final ResourceKey<PlacedFeature> WILDFLOWERS_BIRCH_FOREST = k("wildflowers_birch_forest");
    public static final ResourceKey<PlacedFeature> WILDFLOWERS_MEADOW = k("wildflowers_meadow");
    public static final ResourceKey<PlacedFeature> TREES_PLAINS = k("trees_plains");
    public static final ResourceKey<PlacedFeature> DARK_FOREST_VEGETATION = k("dark_forest_vegetation");
    public static final ResourceKey<PlacedFeature> PALE_GARDEN_VEGETATION = k("pale_garden_vegetation");
    public static final ResourceKey<PlacedFeature> FLOWER_FOREST_FLOWERS = k("flower_forest_flowers");
    public static final ResourceKey<PlacedFeature> FOREST_FLOWERS = k("forest_flowers");
    public static final ResourceKey<PlacedFeature> PALE_GARDEN_FLOWERS = k("pale_garden_flowers");
    public static final ResourceKey<PlacedFeature> PALE_MOSS_PATCH = k("pale_moss_patch");
    public static final ResourceKey<PlacedFeature> TREES_FLOWER_FOREST = k("trees_flower_forest");
    public static final ResourceKey<PlacedFeature> TREES_MEADOW = k("trees_meadow");
    public static final ResourceKey<PlacedFeature> TREES_CHERRY = k("trees_cherry");
    public static final ResourceKey<PlacedFeature> TREES_TAIGA = k("trees_taiga");
    public static final ResourceKey<PlacedFeature> TREES_GROVE = k("trees_grove");
    public static final ResourceKey<PlacedFeature> TREES_BADLANDS = k("trees_badlands");
    public static final ResourceKey<PlacedFeature> TREES_SNOWY = k("trees_snowy");
    public static final ResourceKey<PlacedFeature> TREES_SWAMP = k("trees_swamp");
    public static final ResourceKey<PlacedFeature> TREES_WINDSWEPT_SAVANNA = k("trees_windswept_savanna");
    public static final ResourceKey<PlacedFeature> TREES_SAVANNA = k("trees_savanna");
    public static final ResourceKey<PlacedFeature> BIRCH_TALL = k("birch_tall");
    public static final ResourceKey<PlacedFeature> TREES_BIRCH = k("trees_birch");
    public static final ResourceKey<PlacedFeature> TREES_WINDSWEPT_FOREST = k("trees_windswept_forest");
    public static final ResourceKey<PlacedFeature> TREES_WINDSWEPT_HILLS = k("trees_windswept_hills");
    public static final ResourceKey<PlacedFeature> TREES_WATER = k("trees_water");
    public static final ResourceKey<PlacedFeature> TREES_BIRCH_AND_OAK_LEAF_LITTER = k("trees_birch_and_oak_leaf_litter");
    public static final ResourceKey<PlacedFeature> TREES_SPARSE_JUNGLE = k("trees_sparse_jungle");
    public static final ResourceKey<PlacedFeature> TREES_OLD_GROWTH_SPRUCE_TAIGA = k("trees_old_growth_spruce_taiga");
    public static final ResourceKey<PlacedFeature> TREES_OLD_GROWTH_PINE_TAIGA = k("trees_old_growth_pine_taiga");
    public static final ResourceKey<PlacedFeature> TREES_JUNGLE = k("trees_jungle");
    public static final ResourceKey<PlacedFeature> TREES_DAPPLED_FOREST = k("trees_dappled_forest");
    public static final ResourceKey<PlacedFeature> BAMBOO_VEGETATION = k("bamboo_vegetation");
    public static final ResourceKey<PlacedFeature> MUSHROOM_ISLAND_VEGETATION = k("mushroom_island_vegetation");
    public static final ResourceKey<PlacedFeature> TREES_MANGROVE = k("trees_mangrove");
    public static List<PlacementModifier> worldSurfaceSquaredWithCount(int count) {
        return List.of();
    }
    public static List<PlacementModifier> treePlacement(PlacementModifier frequency) {
        return List.of();
    }
    public static List<PlacementModifier> treePlacement(PlacementModifier frequency, Block sapling) {
        return List.of();
    }
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}

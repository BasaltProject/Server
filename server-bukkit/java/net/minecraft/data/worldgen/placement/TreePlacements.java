// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class TreePlacements {
    public static final ResourceKey<PlacedFeature> CRIMSON_FUNGI = k("crimson_fungi");
    public static final ResourceKey<PlacedFeature> WARPED_FUNGI = k("warped_fungi");
    public static final ResourceKey<PlacedFeature> OAK_CHECKED = k("oak_checked");
    public static final ResourceKey<PlacedFeature> DARK_OAK_CHECKED = k("dark_oak_checked");
    public static final ResourceKey<PlacedFeature> PALE_OAK_CHECKED = k("pale_oak_checked");
    public static final ResourceKey<PlacedFeature> PALE_OAK_CREAKING_CHECKED = k("pale_oak_creaking_checked");
    public static final ResourceKey<PlacedFeature> BIRCH_CHECKED = k("birch_checked");
    public static final ResourceKey<PlacedFeature> ACACIA_CHECKED = k("acacia_checked");
    public static final ResourceKey<PlacedFeature> SPRUCE_CHECKED = k("spruce_checked");
    public static final ResourceKey<PlacedFeature> MANGROVE_CHECKED = k("mangrove_checked");
    public static final ResourceKey<PlacedFeature> CHERRY_CHECKED = k("cherry_checked");
    public static final ResourceKey<PlacedFeature> PINE_ON_SNOW = k("pine_on_snow");
    public static final ResourceKey<PlacedFeature> SPRUCE_ON_SNOW = k("spruce_on_snow");
    public static final ResourceKey<PlacedFeature> PINE_CHECKED = k("pine_checked");
    public static final ResourceKey<PlacedFeature> JUNGLE_TREE_CHECKED = k("jungle_tree");
    public static final ResourceKey<PlacedFeature> FANCY_OAK_CHECKED = k("fancy_oak_checked");
    public static final ResourceKey<PlacedFeature> MEGA_JUNGLE_TREE_CHECKED = k("mega_jungle_tree_checked");
    public static final ResourceKey<PlacedFeature> MEGA_SPRUCE_CHECKED = k("mega_spruce_checked");
    public static final ResourceKey<PlacedFeature> MEGA_PINE_CHECKED = k("mega_pine_checked");
    public static final ResourceKey<PlacedFeature> TALL_MANGROVE_CHECKED = k("tall_mangrove_checked");
    public static final ResourceKey<PlacedFeature> JUNGLE_BUSH = k("jungle_bush");
    public static final ResourceKey<PlacedFeature> SUPER_BIRCH_BEES_0002 = k("super_birch_bees_0002");
    public static final ResourceKey<PlacedFeature> SUPER_BIRCH_BEES = k("super_birch_bees");
    public static final ResourceKey<PlacedFeature> OAK_BEES_0002_LEAF_LITTER = k("oak_bees_0002_leaf_litter");
    public static final ResourceKey<PlacedFeature> OAK_BEES_002 = k("oak_bees_002");
    public static final ResourceKey<PlacedFeature> BIRCH_BEES_0002_PLACED = k("birch_bees_0002");
    public static final ResourceKey<PlacedFeature> BIRCH_BEES_0002_LEAF_LITTER = k("birch_bees_0002_leaf_litter");
    public static final ResourceKey<PlacedFeature> BIRCH_BEES_002 = k("birch_bees_002");
    public static final ResourceKey<PlacedFeature> FANCY_OAK_BEES_0002_LEAF_LITTER = k("fancy_oak_bees_0002_leaf_litter");
    public static final ResourceKey<PlacedFeature> FANCY_OAK_BEES_002 = k("fancy_oak_bees_002");
    public static final ResourceKey<PlacedFeature> FANCY_OAK_BEES = k("fancy_oak_bees");
    public static final ResourceKey<PlacedFeature> CHERRY_BEES_005 = k("cherry_bees_005");
    public static final ResourceKey<PlacedFeature> OAK_LEAF_LITTER = k("oak_leaf_litter");
    public static final ResourceKey<PlacedFeature> DARK_OAK_LEAF_LITTER = k("dark_oak_leaf_litter");
    public static final ResourceKey<PlacedFeature> BIRCH_LEAF_LITTER = k("birch_leaf_litter");
    public static final ResourceKey<PlacedFeature> FANCY_OAK_LEAF_LITTER = k("fancy_oak_leaf_litter");
    public static final ResourceKey<PlacedFeature> RED_POPLAR = k("red_poplar");
    public static final ResourceKey<PlacedFeature> ORANGE_POPLAR = k("orange_poplar");
    public static final ResourceKey<PlacedFeature> YELLOW_POPLAR = k("yellow_poplar");
    public static final ResourceKey<PlacedFeature> RED_POPLAR_LEAF_LITTER = k("red_poplar_leaf_litter");
    public static final ResourceKey<PlacedFeature> ORANGE_POPLAR_LEAF_LITTER = k("orange_poplar_leaf_litter");
    public static final ResourceKey<PlacedFeature> YELLOW_POPLAR_LEAF_LITTER = k("yellow_poplar_leaf_litter");
    public static final ResourceKey<PlacedFeature> FALLEN_OAK_TREE = k("fallen_oak_tree");
    public static final ResourceKey<PlacedFeature> FALLEN_BIRCH_TREE = k("fallen_birch_tree");
    public static final ResourceKey<PlacedFeature> FALLEN_SUPER_BIRCH_TREE = k("fallen_super_birch_tree");
    public static final ResourceKey<PlacedFeature> FALLEN_SPRUCE_TREE = k("fallen_spruce_tree");
    public static final ResourceKey<PlacedFeature> FALLEN_JUNGLE_TREE = k("fallen_jungle_tree");
    public static final ResourceKey<PlacedFeature> FALLEN_POPLAR_TREE = k("fallen_poplar_tree");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}

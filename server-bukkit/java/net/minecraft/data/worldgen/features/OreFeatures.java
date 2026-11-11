// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.feature.Feature;
public class OreFeatures {
    public static final ResourceKey<Feature> ORE_MAGMA = k("ore_magma");
    public static final ResourceKey<Feature> ORE_SOUL_SAND = k("ore_soul_sand");
    public static final ResourceKey<Feature> ORE_NETHER_GOLD = k("ore_nether_gold");
    public static final ResourceKey<Feature> ORE_QUARTZ = k("ore_quartz");
    public static final ResourceKey<Feature> ORE_GRAVEL_NETHER = k("ore_gravel_nether");
    public static final ResourceKey<Feature> ORE_BLACKSTONE = k("ore_blackstone");
    public static final ResourceKey<Feature> ORE_DIRT = k("ore_dirt");
    public static final ResourceKey<Feature> ORE_GRAVEL = k("ore_gravel");
    public static final ResourceKey<Feature> ORE_GRANITE = k("ore_granite");
    public static final ResourceKey<Feature> ORE_DIORITE = k("ore_diorite");
    public static final ResourceKey<Feature> ORE_ANDESITE = k("ore_andesite");
    public static final ResourceKey<Feature> ORE_TUFF = k("ore_tuff");
    public static final ResourceKey<Feature> ORE_COAL = k("ore_coal");
    public static final ResourceKey<Feature> ORE_COAL_BURIED = k("ore_coal_buried");
    public static final ResourceKey<Feature> ORE_IRON = k("ore_iron");
    public static final ResourceKey<Feature> ORE_IRON_SMALL = k("ore_iron_small");
    public static final ResourceKey<Feature> ORE_GOLD = k("ore_gold");
    public static final ResourceKey<Feature> ORE_GOLD_BURIED = k("ore_gold_buried");
    public static final ResourceKey<Feature> ORE_REDSTONE = k("ore_redstone");
    public static final ResourceKey<Feature> ORE_DIAMOND_SMALL = k("ore_diamond_small");
    public static final ResourceKey<Feature> ORE_DIAMOND_MEDIUM = k("ore_diamond_medium");
    public static final ResourceKey<Feature> ORE_DIAMOND_LARGE = k("ore_diamond_large");
    public static final ResourceKey<Feature> ORE_DIAMOND_BURIED = k("ore_diamond_buried");
    public static final ResourceKey<Feature> ORE_LAPIS = k("ore_lapis");
    public static final ResourceKey<Feature> ORE_LAPIS_BURIED = k("ore_lapis_buried");
    public static final ResourceKey<Feature> ORE_INFESTED = k("ore_infested");
    public static final ResourceKey<Feature> ORE_EMERALD = k("ore_emerald");
    public static final ResourceKey<Feature> ORE_ANCIENT_DEBRIS_LARGE = k("ore_ancient_debris_large");
    public static final ResourceKey<Feature> ORE_ANCIENT_DEBRIS_SMALL = k("ore_ancient_debris_small");
    public static final ResourceKey<Feature> ORE_COPPPER_SMALL = k("ore_copper_small");
    public static final ResourceKey<Feature> ORE_COPPER_LARGE = k("ore_copper_large");
    public static final ResourceKey<Feature> ORE_CLAY = k("ore_clay");
    public static void bootstrap(BootstrapContext<Feature> context) {
    }

    private static ResourceKey<Feature> k(String n) {
        return FeatureUtils.createKey(n);
    }
}

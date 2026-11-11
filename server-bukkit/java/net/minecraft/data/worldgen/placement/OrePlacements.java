// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class OrePlacements {
    public static final ResourceKey<PlacedFeature> ORE_MAGMA = k("ore_magma");
    public static final ResourceKey<PlacedFeature> ORE_SOUL_SAND = k("ore_soul_sand");
    public static final ResourceKey<PlacedFeature> ORE_GOLD_DELTAS = k("ore_gold_deltas");
    public static final ResourceKey<PlacedFeature> ORE_QUARTZ_DELTAS = k("ore_quartz_deltas");
    public static final ResourceKey<PlacedFeature> ORE_GOLD_NETHER = k("ore_gold_nether");
    public static final ResourceKey<PlacedFeature> ORE_QUARTZ_NETHER = k("ore_quartz_nether");
    public static final ResourceKey<PlacedFeature> ORE_GRAVEL_NETHER = k("ore_gravel_nether");
    public static final ResourceKey<PlacedFeature> ORE_BLACKSTONE = k("ore_blackstone");
    public static final ResourceKey<PlacedFeature> ORE_DIRT = k("ore_dirt");
    public static final ResourceKey<PlacedFeature> ORE_GRAVEL = k("ore_gravel");
    public static final ResourceKey<PlacedFeature> ORE_GRANITE_UPPER = k("ore_granite_upper");
    public static final ResourceKey<PlacedFeature> ORE_GRANITE_LOWER = k("ore_granite_lower");
    public static final ResourceKey<PlacedFeature> ORE_DIORITE_UPPER = k("ore_diorite_upper");
    public static final ResourceKey<PlacedFeature> ORE_DIORITE_LOWER = k("ore_diorite_lower");
    public static final ResourceKey<PlacedFeature> ORE_ANDESITE_UPPER = k("ore_andesite_upper");
    public static final ResourceKey<PlacedFeature> ORE_ANDESITE_LOWER = k("ore_andesite_lower");
    public static final ResourceKey<PlacedFeature> ORE_TUFF = k("ore_tuff");
    public static final ResourceKey<PlacedFeature> ORE_COAL_UPPER = k("ore_coal_upper");
    public static final ResourceKey<PlacedFeature> ORE_COAL_LOWER = k("ore_coal_lower");
    public static final ResourceKey<PlacedFeature> ORE_IRON_UPPER = k("ore_iron_upper");
    public static final ResourceKey<PlacedFeature> ORE_IRON_MIDDLE = k("ore_iron_middle");
    public static final ResourceKey<PlacedFeature> ORE_IRON_SMALL = k("ore_iron_small");
    public static final ResourceKey<PlacedFeature> ORE_GOLD_EXTRA = k("ore_gold_extra");
    public static final ResourceKey<PlacedFeature> ORE_GOLD = k("ore_gold");
    public static final ResourceKey<PlacedFeature> ORE_GOLD_LOWER = k("ore_gold_lower");
    public static final ResourceKey<PlacedFeature> ORE_REDSTONE = k("ore_redstone");
    public static final ResourceKey<PlacedFeature> ORE_REDSTONE_LOWER = k("ore_redstone_lower");
    public static final ResourceKey<PlacedFeature> ORE_DIAMOND = k("ore_diamond");
    public static final ResourceKey<PlacedFeature> ORE_DIAMOND_MEDIUM = k("ore_diamond_medium");
    public static final ResourceKey<PlacedFeature> ORE_DIAMOND_LARGE = k("ore_diamond_large");
    public static final ResourceKey<PlacedFeature> ORE_DIAMOND_BURIED = k("ore_diamond_buried");
    public static final ResourceKey<PlacedFeature> ORE_LAPIS = k("ore_lapis");
    public static final ResourceKey<PlacedFeature> ORE_LAPIS_BURIED = k("ore_lapis_buried");
    public static final ResourceKey<PlacedFeature> ORE_INFESTED = k("ore_infested");
    public static final ResourceKey<PlacedFeature> ORE_EMERALD = k("ore_emerald");
    public static final ResourceKey<PlacedFeature> ORE_ANCIENT_DEBRIS_LARGE = k("ore_ancient_debris_large");
    public static final ResourceKey<PlacedFeature> ORE_ANCIENT_DEBRIS_SMALL = k("ore_debris_small");
    public static final ResourceKey<PlacedFeature> ORE_COPPER = k("ore_copper");
    public static final ResourceKey<PlacedFeature> ORE_COPPER_LARGE = k("ore_copper_large");
    public static final ResourceKey<PlacedFeature> ORE_CLAY = k("ore_clay");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}

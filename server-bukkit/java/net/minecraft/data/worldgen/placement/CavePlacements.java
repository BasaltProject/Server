// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class CavePlacements {
    public static final ResourceKey<PlacedFeature> MONSTER_ROOM = k("monster_room");
    public static final ResourceKey<PlacedFeature> MONSTER_ROOM_DEEP = k("monster_room_deep");
    public static final ResourceKey<PlacedFeature> FOSSIL_UPPER = k("fossil_upper");
    public static final ResourceKey<PlacedFeature> FOSSIL_LOWER = k("fossil_lower");
    public static final ResourceKey<PlacedFeature> DRIPSTONE_CLUSTER = k("dripstone_cluster");
    public static final ResourceKey<PlacedFeature> SULFUR_SPIKE_CLUSTER = k("sulfur_spike_cluster");
    public static final ResourceKey<PlacedFeature> LARGE_DRIPSTONE = k("large_dripstone");
    public static final ResourceKey<PlacedFeature> POINTED_DRIPSTONE = k("pointed_dripstone");
    public static final ResourceKey<PlacedFeature> SULFUR_SPIKE = k("sulfur_spike");
    public static final ResourceKey<PlacedFeature> UNDERWATER_MAGMA = k("underwater_magma");
    public static final ResourceKey<PlacedFeature> GLOW_LICHEN = k("glow_lichen");
    public static final ResourceKey<PlacedFeature> ROOTED_AZALEA_TREE = k("rooted_azalea_tree");
    public static final ResourceKey<PlacedFeature> ROOTED_SULFUR_SPRING = k("rooted_sulfur_spring");
    public static final ResourceKey<PlacedFeature> CAVE_VINES = k("cave_vines");
    public static final ResourceKey<PlacedFeature> LUSH_CAVES_VEGETATION = k("lush_caves_vegetation");
    public static final ResourceKey<PlacedFeature> LUSH_CAVES_CLAY = k("lush_caves_clay");
    public static final ResourceKey<PlacedFeature> LUSH_CAVES_CEILING_VEGETATION = k("lush_caves_ceiling_vegetation");
    public static final ResourceKey<PlacedFeature> SPORE_BLOSSOM = k("spore_blossom");
    public static final ResourceKey<PlacedFeature> CLASSIC_VINES = k("classic_vines_cave_feature");
    public static final ResourceKey<PlacedFeature> AMETHYST_GEODE = k("amethyst_geode");
    public static final ResourceKey<PlacedFeature> SCULK_PATCH_DEEP_DARK = k("sculk_patch_deep_dark");
    public static final ResourceKey<PlacedFeature> SCULK_PATCH_ANCIENT_CITY = k("sculk_patch_ancient_city");
    public static final ResourceKey<PlacedFeature> SCULK_VEIN = k("sculk_vein");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class MiscOverworldPlacements {
    public static final ResourceKey<PlacedFeature> ICE_SPIKE = k("ice_spike");
    public static final ResourceKey<PlacedFeature> ICE_PATCH = k("ice_patch");
    public static final ResourceKey<PlacedFeature> FOREST_ROCK = k("forest_rock");
    public static final ResourceKey<PlacedFeature> ICEBERG_PACKED = k("iceberg_packed");
    public static final ResourceKey<PlacedFeature> ICEBERG_BLUE = k("iceberg_blue");
    public static final ResourceKey<PlacedFeature> BLUE_ICE = k("blue_ice");
    public static final ResourceKey<PlacedFeature> LAKE_LAVA_UNDERGROUND = k("lake_lava_underground");
    public static final ResourceKey<PlacedFeature> LAKE_LAVA_SURFACE = k("lake_lava_surface");
    public static final ResourceKey<PlacedFeature> SULFUR_POOL = k("sulfur_pool");
    public static final ResourceKey<PlacedFeature> DISK_CLAY = k("disk_clay");
    public static final ResourceKey<PlacedFeature> DISK_GRAVEL = k("disk_gravel");
    public static final ResourceKey<PlacedFeature> DISK_SAND = k("disk_sand");
    public static final ResourceKey<PlacedFeature> DISK_GRASS = k("disk_grass");
    public static final ResourceKey<PlacedFeature> FREEZE_TOP_LAYER = k("freeze_top_layer");
    public static final ResourceKey<PlacedFeature> VOID_START_PLATFORM = k("void_start_platform");
    public static final ResourceKey<PlacedFeature> DESERT_WELL = k("desert_well");
    public static final ResourceKey<PlacedFeature> SPRING_LAVA = k("spring_lava");
    public static final ResourceKey<PlacedFeature> SPRING_LAVA_FROZEN = k("spring_lava_frozen");
    public static final ResourceKey<PlacedFeature> SPRING_WATER = k("spring_water");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}

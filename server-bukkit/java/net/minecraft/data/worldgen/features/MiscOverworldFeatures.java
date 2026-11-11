// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.feature.Feature;
public class MiscOverworldFeatures {
    public static final ResourceKey<Feature> ICE_SPIKE = k("ice_spike");
    public static final ResourceKey<Feature> ICE_PATCH = k("ice_patch");
    public static final ResourceKey<Feature> FOREST_ROCK = k("forest_rock");
    public static final ResourceKey<Feature> ICEBERG_PACKED = k("iceberg_packed");
    public static final ResourceKey<Feature> ICEBERG_BLUE = k("iceberg_blue");
    public static final ResourceKey<Feature> BLUE_ICE = k("blue_ice");
    public static final ResourceKey<Feature> LAKE_LAVA = k("lake_lava");
    public static final ResourceKey<Feature> SULFUR_POOL = k("sulfur_pool");
    public static final ResourceKey<Feature> SULFUR_SPRING = k("sulfur_spring");
    public static final ResourceKey<Feature> DISK_CLAY = k("disk_clay");
    public static final ResourceKey<Feature> DISK_GRAVEL = k("disk_gravel");
    public static final ResourceKey<Feature> DISK_SAND = k("disk_sand");
    public static final ResourceKey<Feature> FREEZE_TOP_LAYER = k("freeze_top_layer");
    public static final ResourceKey<Feature> DISK_GRASS = k("disk_grass");
    public static final ResourceKey<Feature> BONUS_CHEST = k("bonus_chest");
    public static final ResourceKey<Feature> VOID_START_PLATFORM = k("void_start_platform");
    public static final ResourceKey<Feature> DESERT_WELL = k("desert_well");
    public static final ResourceKey<Feature> SPRING_LAVA_OVERWORLD = k("spring_lava_overworld");
    public static final ResourceKey<Feature> SPRING_LAVA_FROZEN = k("spring_lava_frozen");
    public static final ResourceKey<Feature> SPRING_WATER = k("spring_water");
    public static void bootstrap(BootstrapContext<Feature> context) {
    }

    private static ResourceKey<Feature> k(String n) {
        return FeatureUtils.createKey(n);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;

import net.minecraft.resources.ResourceKey;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.feature.Feature;

public class NetherFeatures {
    protected NetherFeatures() {}

    public static final ResourceKey<Object> DELTA = FeatureUtils.createKey("delta");
    public static final ResourceKey<Object> SMALL_BASALT_COLUMNS = FeatureUtils.createKey("small_basalt_columns");
    public static final ResourceKey<Object> LARGE_BASALT_COLUMNS = FeatureUtils.createKey("large_basalt_columns");
    public static final ResourceKey<Object> BASALT_BLOBS = FeatureUtils.createKey("basalt_blobs");
    public static final ResourceKey<Object> BLACKSTONE_BLOBS = FeatureUtils.createKey("blackstone_blobs");
    public static final ResourceKey<Object> GLOWSTONE_EXTRA = FeatureUtils.createKey("glowstone_extra");
    public static final ResourceKey<Object> CRIMSON_FOREST_VEGETATION = FeatureUtils.createKey("crimson_forest_vegetation");
    public static final ResourceKey<Object> WARPED_FOREST_VEGETION = FeatureUtils.createKey("warped_forest_vegetation");
    public static final ResourceKey<Object> NETHER_SPROUTS = FeatureUtils.createKey("nether_sprouts");
    public static final ResourceKey<Object> TWISTING_VINES = FeatureUtils.createKey("twisting_vines");
    public static final ResourceKey<Object> WEEPING_VINES = FeatureUtils.createKey("weeping_vines");
    public static final ResourceKey<Object> CRIMSON_ROOTS = FeatureUtils.createKey("crimson_roots");
    public static final ResourceKey<Object> BASALT_PILLAR = FeatureUtils.createKey("basalt_pillar");
    public static final ResourceKey<Object> SPRING_LAVA_NETHER = FeatureUtils.createKey("spring_lava_nether");
    public static final ResourceKey<Object> SPRING_NETHER_CLOSED = FeatureUtils.createKey("spring_nether_closed");
    public static final ResourceKey<Object> SPRING_NETHER_OPEN = FeatureUtils.createKey("spring_nether_open");
    public static final ResourceKey<Object> FIRE = FeatureUtils.createKey("patch_fire");
    public static final ResourceKey<Object> SOUL_FIRE = FeatureUtils.createKey("patch_soul_fire");
    public static final ResourceKey<Object> NYLIUM_BONEMEAL = FeatureUtils.createKey("nylium_bonemeal");
    public static void bootstrap(BootstrapContext<Feature> context) {
        // registries come from the native datapack loader, so there is nothing to bootstrap
    }
}

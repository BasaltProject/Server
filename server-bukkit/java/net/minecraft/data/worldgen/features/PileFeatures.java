// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;

import net.minecraft.resources.ResourceKey;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.feature.Feature;

public class PileFeatures {
    protected PileFeatures() {}

    public static final ResourceKey<Object> PILE_HAY = FeatureUtils.createKey("pile_hay");
    public static final ResourceKey<Object> PILE_MELON = FeatureUtils.createKey("pile_melon");
    public static final ResourceKey<Object> PILE_SNOW = FeatureUtils.createKey("pile_snow");
    public static final ResourceKey<Object> PILE_ICE = FeatureUtils.createKey("pile_ice");
    public static final ResourceKey<Object> PILE_PUMPKIN = FeatureUtils.createKey("pile_pumpkin");
    public static void bootstrap(BootstrapContext<Feature> context) {
        // registries come from the native datapack loader, so there is nothing to bootstrap
    }
}

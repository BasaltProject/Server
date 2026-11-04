// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;

import net.minecraft.resources.ResourceKey;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.feature.Feature;

public class AquaticFeatures {
    protected AquaticFeatures() {}

    public static final ResourceKey<Object> SEAGRASS_SHORT = FeatureUtils.createKey("seagrass_short");
    public static final ResourceKey<Object> SEAGRASS_SLIGHTLY_LESS_SHORT = FeatureUtils.createKey("seagrass_slightly_less_short");
    public static final ResourceKey<Object> SEAGRASS_MID = FeatureUtils.createKey("seagrass_mid");
    public static final ResourceKey<Object> SEAGRASS_TALL = FeatureUtils.createKey("seagrass_tall");
    public static final ResourceKey<Object> SEA_PICKLE = FeatureUtils.createKey("sea_pickle");
    public static final ResourceKey<Object> KELP = FeatureUtils.createKey("kelp");
    public static final ResourceKey<Object> WARM_OCEAN_VEGETATION = FeatureUtils.createKey("warm_ocean_vegetation");
    public static final ResourceKey<Object> CORAL_BLOCK_DECORATION = FeatureUtils.createKey("coral/block_decoration");
    public static final ResourceKey<Object> TUBE_CORAL_BLOCK = FeatureUtils.createKey("coral/tube_block");
    public static final ResourceKey<Object> BRAIN_CORAL_BLOCK = FeatureUtils.createKey("coral/brain_block");
    public static final ResourceKey<Object> BUBBLE_CORAL_BLOCK = FeatureUtils.createKey("coral/bubble_block");
    public static final ResourceKey<Object> FIRE_CORAL_BLOCK = FeatureUtils.createKey("coral/fire_block");
    public static final ResourceKey<Object> HORN_CORAL_BLOCK = FeatureUtils.createKey("coral/horn_block");
    public static void bootstrap(BootstrapContext<Feature> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.data.worldgen.features.AquaticFeatures#bootstrap");
    }
}

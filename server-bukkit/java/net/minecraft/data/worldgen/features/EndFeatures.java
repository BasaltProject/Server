// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;

import net.minecraft.resources.ResourceKey;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.feature.Feature;

public class EndFeatures {
    protected EndFeatures() {}

    public static final ResourceKey<Object> END_PLATFORM = FeatureUtils.createKey("end_platform");
    public static final ResourceKey<Object> END_SPIKE = FeatureUtils.createKey("end_spike");
    public static final ResourceKey<Object> END_GATEWAY_RETURN = FeatureUtils.createKey("end_gateway_return");
    public static final ResourceKey<Object> END_GATEWAY_DELAYED = FeatureUtils.createKey("end_gateway_delayed");
    public static final ResourceKey<Object> CHORUS_PLANT = FeatureUtils.createKey("chorus_plant");
    public static final ResourceKey<Object> END_ISLAND = FeatureUtils.createKey("end_island");
    public static final ResourceKey<Object> END_PODIUM_ACTIVE = FeatureUtils.createKey("end_podium_active");
    public static final ResourceKey<Object> END_PODIUM_INACTIVE = FeatureUtils.createKey("end_podium_inactive");
    public static void bootstrap(BootstrapContext<Feature> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.data.worldgen.features.EndFeatures#bootstrap");
    }
}

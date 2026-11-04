// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.features;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.feature.Feature;

/** Stable keys for the configured-feature registry, independent of its bootstrap order. */
public final class FeatureUtils {
    private FeatureUtils() {}

    public static ResourceKey<Object> createKey(String name) {
        return ResourceKey.create(Registries.CONFIGURED_FEATURE, Identifier.withDefaultNamespace(name));
    }
    public static void bootstrap(BootstrapContext<Feature> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.data.worldgen.features.FeatureUtils#bootstrap");
    }
}

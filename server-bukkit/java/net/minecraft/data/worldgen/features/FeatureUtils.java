package net.minecraft.data.worldgen.features;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

/** Stable keys for the configured-feature registry, independent of its bootstrap order. */
public final class FeatureUtils {
    private FeatureUtils() {}

    public static ResourceKey<Object> createKey(String name) {
        return ResourceKey.create(Registries.CONFIGURED_FEATURE, Identifier.withDefaultNamespace(name));
    }
}

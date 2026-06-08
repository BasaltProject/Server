package net.minecraft.tags;

import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

public record TagKey<T>(ResourceKey<? extends Registry<T>> registry, Identifier location) {
    public static <T> TagKey<T> create(ResourceKey<? extends Registry<T>> registry, Identifier name) {
        return new TagKey<>(registry, name);
    }

    public boolean isFor(ResourceKey<? extends Registry<?>> registryKey) {
        return registry.identifier().equals(registryKey.identifier());
    }

    @Override
    public String toString() {
        return "TagKey[" + registry.identifier() + " / " + location + "]";
    }
}

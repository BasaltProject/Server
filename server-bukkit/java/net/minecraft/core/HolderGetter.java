package net.minecraft.core;

import java.util.Optional;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public interface HolderGetter<T> {
    Optional<Holder.Reference<T>> get(ResourceKey<T> key);

    default Holder.Reference<T> getOrThrow(ResourceKey<T> key) {
        return get(key).orElseThrow(() -> new IllegalStateException("Missing element " + key));
    }

    Optional<HolderSet.Named<T>> get(TagKey<T> key);

    default HolderSet.Named<T> getOrThrow(TagKey<T> key) {
        return get(key).orElseThrow(() -> new IllegalStateException("Missing tag " + key));
    }

    interface Provider {
        <T> Optional<? extends HolderGetter<T>> lookup(ResourceKey<? extends Registry<? extends T>> registryKey);

        default <T> HolderGetter<T> lookupOrThrow(ResourceKey<? extends Registry<? extends T>> registryKey) {
            return this.<T>lookup(registryKey).orElseThrow(() -> new IllegalStateException("Missing registry " + registryKey));
        }
    }
}

package net.minecraft.core;

import java.util.Optional;
import net.minecraft.resources.ResourceKey;

public interface RegistryAccess extends HolderLookup.Provider {
    @SuppressWarnings("unchecked")
    <E> Optional<Registry<E>> lookup(ResourceKey<? extends Registry<? extends E>> key);

    @Override
    @SuppressWarnings("unchecked")
    default <E> Registry<E> lookupOrThrow(ResourceKey<? extends Registry<? extends E>> key) {
        return this.<E>lookup(key).orElseThrow(() -> new IllegalStateException("Missing registry " + key));
    }

    interface Frozen extends RegistryAccess {}
}

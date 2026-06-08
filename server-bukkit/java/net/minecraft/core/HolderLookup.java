package net.minecraft.core;

import com.mojang.serialization.DynamicOps;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;

public interface HolderLookup<T> extends HolderGetter<T> {
    Stream<Holder.Reference<T>> listElements();

    Stream<HolderSet.Named<T>> listTags();

    interface RegistryLookup<T> extends HolderLookup<T> {
        ResourceKey<? extends Registry<? extends T>> key();
    }

    interface Provider extends HolderGetter.Provider {
        @Override
        <T> Optional<? extends HolderLookup.RegistryLookup<T>> lookup(ResourceKey<? extends Registry<? extends T>> key);

        @Override
        default <T> HolderLookup.RegistryLookup<T> lookupOrThrow(ResourceKey<? extends Registry<? extends T>> key) {
            return this.<T>lookup(key).orElseThrow(() -> new IllegalStateException("Missing registry " + key));
        }

        default <V> RegistryOps<V> createSerializationContext(DynamicOps<V> parent) {
            return RegistryOps.create(parent, this);
        }
    }
}

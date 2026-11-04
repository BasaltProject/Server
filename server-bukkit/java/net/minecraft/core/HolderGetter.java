// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.Optional;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;

public interface HolderGetter<T> extends HolderOwner<T> {
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
            default <T> Optional<Holder.Reference<T>> get(ResourceKey<T> id) {
        return this.<T>lookup(id.registryKey()).flatMap(lookup -> lookup.get(id));
        }
        default <T> Holder.Reference<T> getOrThrow(ResourceKey<T> id) {
        return this.<T>get(id).orElseThrow(() -> new IllegalStateException("Missing element " + id));
        }
        default <T> Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return this.<T>lookup(id.registry()).flatMap(lookup -> lookup.get(id));
        }
        default <T> HolderSet.Named<T> getOrThrow(TagKey<T> id) {
        return this.<T>get(id).orElseThrow(() -> new IllegalStateException("Missing tag " + id));
        }
}
    default Optional<Holder<T>> getRandomElementOf(TagKey<T> tag, RandomSource random) {
        return get(tag).flatMap(set -> set.getRandomElement(random));
    }
}

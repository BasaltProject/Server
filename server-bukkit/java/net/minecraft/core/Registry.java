package net.minecraft.core;

import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public interface Registry<T> extends IdMap<T>, HolderLookup.RegistryLookup<T> {
    @Override
    ResourceKey<? extends Registry<T>> key();

    Identifier getKey(T value);

    Optional<ResourceKey<T>> getResourceKey(T value);

    T getValue(ResourceKey<T> key);

    T getValue(Identifier id);

    default T getValueOrThrow(ResourceKey<T> key) {
        T value = getValue(key);
        if (value == null) {
            throw new IllegalStateException("Missing key in " + key() + ": " + key);
        }
        return value;
    }

    default Optional<T> getOptional(Identifier id) {
        return Optional.ofNullable(getValue(id));
    }

    default Optional<T> getOptional(ResourceKey<T> key) {
        return Optional.ofNullable(getValue(key));
    }

    Optional<Holder.Reference<T>> get(Identifier id);

    @Override
    Optional<Holder.Reference<T>> get(ResourceKey<T> key);

    Optional<Holder.Reference<T>> get(int id);

    @Override
    default Holder.Reference<T> getOrThrow(ResourceKey<T> key) {
        return get(key).orElseThrow(() -> new IllegalStateException("Missing key in " + key() + ": " + key));
    }

    Set<Identifier> keySet();

    Set<ResourceKey<T>> registryKeySet();

    Holder<T> wrapAsHolder(T value);

    IdMap<Holder<T>> asHolderIdMap();

    @Override
    Optional<HolderSet.Named<T>> get(TagKey<T> tag);

    Stream<HolderSet.Named<T>> getTags();

    boolean containsKey(Identifier id);

    default Stream<T> stream() {
        return StreamSupport.stream(spliterator(), false);
    }
}

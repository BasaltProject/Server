package org.basaltmc.bukkit;

import io.papermc.paper.registry.tag.Tag;
import io.papermc.paper.registry.tag.TagKey;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.function.Supplier;
import java.util.stream.Stream;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;

public final class BasaltRegistry<T extends Keyed> implements Registry<T> {
    private final Supplier<List<T>> source;
    private Map<NamespacedKey, T> entries;

    BasaltRegistry(Supplier<List<T>> source) {
        this.source = source;
    }

    private synchronized Map<NamespacedKey, T> entries() {
        if (entries == null) {
            Map<NamespacedKey, T> built = new LinkedHashMap<>();
            for (T entry : source.get()) {
                built.put(entry.getKey(), entry);
            }
            entries = built;
        }
        return entries;
    }

    @Override
    public T get(NamespacedKey key) {
        return entries().get(key);
    }

    @Override
    public NamespacedKey getKey(T value) {
        return value.getKey();
    }

    @Override
    public boolean hasTag(TagKey<T> key) {
        return false;
    }

    @Override
    public Tag<T> getTag(TagKey<T> key) {
        throw new NoSuchElementException("No tag " + key);
    }

    @Override
    public Collection<Tag<T>> getTags() {
        return List.of();
    }

    @Override
    public Stream<T> stream() {
        return entries().values().stream();
    }

    @Override
    public Stream<NamespacedKey> keyStream() {
        return entries().keySet().stream();
    }

    @Override
    public int size() {
        return entries().size();
    }

    @Override
    public Iterator<T> iterator() {
        return entries().values().iterator();
    }
}

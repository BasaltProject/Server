package org.basaltmc.bukkit;

import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.tag.Tag;
import io.papermc.paper.registry.tag.TagKey;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.stream.Stream;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;

public final class BasaltRegistry<T extends Keyed> implements Registry<T> {
    private final Supplier<List<T>> source;
    private final Function<NamespacedKey, T> open;
    private final List<String> known;
    private Map<NamespacedKey, T> entries;
    private boolean listed;
    private String name;
    private Map<NamespacedKey, Tag<T>> tags;

    BasaltRegistry(Supplier<List<T>> source) {
        this(source, null);
    }

    /** A registry the server cannot list at all still answers for a key a plugin names, through {@code open}. */
    BasaltRegistry(Supplier<List<T>> source, Function<NamespacedKey, T> open) {
        this(source, open, null);
    }

    /** A registry whose entries the server cannot list is told the vanilla ones, which {@code open} then makes. */
    BasaltRegistry(Supplier<List<T>> source, Function<NamespacedKey, T> open, List<String> known) {
        this.source = source;
        this.open = open;
        this.known = known;
    }

    BasaltRegistry<T> named(String registryName) {
        this.name = registryName;
        return this;
    }

    private static String tagSource(String registry) {
        if (registry == null) {
            return null;
        }
        return switch (registry) {
            case "block" -> "blocks";
            case "item" -> "items";
            case "entity_type" -> "entity_types";
            case "fluid" -> "fluids";
            default -> null;
        };
    }

    @SuppressWarnings("unchecked")
    private synchronized Map<NamespacedKey, Tag<T>> tags(RegistryKey<T> registry) {
        if (tags == null) {
            tags = new LinkedHashMap<>();
            String source = tagSource(name);
            if (source != null) {
                String[] flat = WorldNatives.tags(source);
                for (int i = 0; i + 1 < flat.length; i += 2) {
                    NamespacedKey id = NamespacedKey.fromString(flat[i]);
                    List<String> members = flat[i + 1].isEmpty() ? List.of() : List.of(flat[i + 1].split("\\|"));
                    tags.put(id, new BasaltRegistryTag<>(registry, registry.tagKey(id), members));
                }
            }
        }
        return tags;
    }

    @SuppressWarnings("unchecked")
    private RegistryKey<T> registryKey() {
        try {
            for (java.lang.reflect.Field field : RegistryKey.class.getFields()) {
                if (field.get(null) instanceof RegistryKey<?> candidate && candidate.key().value().equals(name)) {
                    return (RegistryKey<T>) candidate;
                }
            }
        } catch (IllegalAccessException failure) {
            throw new IllegalStateException(failure);
        }
        throw new IllegalStateException("No registry key for " + name);
    }

    private synchronized Map<NamespacedKey, T> entries() {
        if (entries == null) {
            Map<NamespacedKey, T> built = new LinkedHashMap<>();
            for (T entry : source.get()) {
                built.put(entry.getKey(), entry);
            }
            listed = !built.isEmpty();
            entries = built;
            if (!listed && open != null && known != null) {
                for (String id : known) {
                    NamespacedKey key = NamespacedKey.minecraft(id);
                    T made = open.apply(key);
                    if (made != null) {
                        built.put(key, made);
                    }
                }
                listed = true;
            }
        }
        return entries;
    }

    public synchronized void addEntry(T entry) {
        entries().put(entry.getKey(), entry);
    }

    @Override
    public T get(NamespacedKey key) {
        T found = entries().get(key);
        if (found == null && open != null && !listed) {
            synchronized (this) {
                found = entries().get(key);
                if (found == null) {
                    found = open.apply(key);
                    if (found != null) {
                        entries().put(key, found);
                    }
                }
            }
        }
        return found;
    }

    @Override
    public NamespacedKey getKey(T value) {
        return value.getKey();
    }

    @Override
    public boolean hasTag(TagKey<T> key) {
        return tags(key.registryKey()).containsKey(new NamespacedKey(key.key().namespace(), key.key().value()));
    }

    @Override
    public Tag<T> getTag(TagKey<T> key) {
        Tag<T> found = tags(key.registryKey()).get(new NamespacedKey(key.key().namespace(), key.key().value()));
        if (found == null) {
            throw new NoSuchElementException("No tag " + key);
        }
        return found;
    }

    @Override
    public Collection<Tag<T>> getTags() {
        return List.copyOf(tags(registryKey()).values());
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

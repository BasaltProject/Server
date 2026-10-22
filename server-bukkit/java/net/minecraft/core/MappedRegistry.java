package net.minecraft.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public class MappedRegistry<T> implements WritableRegistry<T> {
    private final ResourceKey<? extends Registry<T>> key;
    private final List<Holder.Reference<T>> byId = new ArrayList<>();
    private final Map<Identifier, Holder.Reference<T>> byLocation = new LinkedHashMap<>();
    private final Map<ResourceKey<T>, Holder.Reference<T>> byKey = new LinkedHashMap<>();
    private final Map<T, Holder.Reference<T>> byValue = new IdentityHashMap<>();
    private final Map<TagKey<T>, HolderSet.Named<T>> tags = new LinkedHashMap<>();
    private final IdMap<Holder<T>> holderIds = new HolderIds();
    private Runnable tagLoader;

    public MappedRegistry(ResourceKey<? extends Registry<T>> key) {
        this.key = key;
    }

    @Override
    public synchronized Holder.Reference<T> register(ResourceKey<T> resourceKey, T value) {
        Holder.Reference<T> holder = new Holder.Reference<>(this, resourceKey, value);
        byId.add(holder);
        byLocation.put(resourceKey.identifier(), holder);
        byKey.put(resourceKey, holder);
        byValue.put(value, holder);
        return holder;
    }

    public Holder.Reference<T> register(Identifier id, T value) {
        return register(ResourceKey.create(key, id), value);
    }

    public synchronized void tagsFrom(Runnable loader) {
        tagLoader = loader;
    }

    private synchronized void loadTags() {
        if (tagLoader != null) {
            Runnable loader = tagLoader;
            tagLoader = null;
            loader.run();
        }
    }

    public synchronized void bindTag(TagKey<T> tag, List<Holder<T>> holders) {
        HolderSet.Named<T> set = new HolderSet.Named<>(tag);
        set.bind(holders);
        tags.put(tag, set);
    }

    @Override
    public ResourceKey<? extends Registry<T>> key() {
        return key;
    }

    @Override
    public Identifier getKey(T value) {
        Holder.Reference<T> holder = byValue.get(value);
        return holder == null ? null : holder.key().identifier();
    }

    @Override
    public Optional<ResourceKey<T>> getResourceKey(T value) {
        Holder.Reference<T> holder = byValue.get(value);
        return holder == null ? Optional.empty() : Optional.of(holder.key());
    }

    @Override
    public int getId(T value) {
        Holder.Reference<T> holder = byValue.get(value);
        return holder == null ? -1 : byId.indexOf(holder);
    }

    @Override
    public T byId(int id) {
        return id >= 0 && id < byId.size() ? byId.get(id).value() : null;
    }

    @Override
    public int size() {
        return byId.size();
    }

    @Override
    public T getValue(ResourceKey<T> resourceKey) {
        Holder.Reference<T> holder = byKey.get(resourceKey);
        return holder == null ? null : holder.value();
    }

    @Override
    public T getValue(Identifier id) {
        Holder.Reference<T> holder = byLocation.get(id);
        return holder == null ? null : holder.value();
    }

    @Override
    public Optional<Holder.Reference<T>> get(Identifier id) {
        return Optional.ofNullable(byLocation.get(id));
    }

    @Override
    public Optional<Holder.Reference<T>> get(ResourceKey<T> resourceKey) {
        return Optional.ofNullable(byKey.get(resourceKey));
    }

    @Override
    public Optional<Holder.Reference<T>> get(int id) {
        return id >= 0 && id < byId.size() ? Optional.of(byId.get(id)) : Optional.empty();
    }

    @Override
    public Set<Identifier> keySet() {
        return Collections.unmodifiableSet(byLocation.keySet());
    }

    @Override
    public Set<ResourceKey<T>> registryKeySet() {
        return Collections.unmodifiableSet(byKey.keySet());
    }

    @Override
    public boolean containsKey(Identifier id) {
        return byLocation.containsKey(id);
    }

    @Override
    public Holder<T> wrapAsHolder(T value) {
        Holder.Reference<T> holder = byValue.get(value);
        return holder != null ? holder : Holder.direct(value);
    }

    @Override
    public IdMap<Holder<T>> asHolderIdMap() {
        return holderIds;
    }

    @Override
    public Optional<HolderSet.Named<T>> get(TagKey<T> tag) {
        loadTags();
        return Optional.ofNullable(tags.get(tag));
    }

    @Override
    public Stream<HolderSet.Named<T>> getTags() {
        loadTags();
        return tags.values().stream();
    }

    @Override
    public Stream<Holder.Reference<T>> listElements() {
        return byId.stream();
    }

    @Override
    public Stream<HolderSet.Named<T>> listTags() {
        return getTags();
    }

    @Override
    public Iterator<T> iterator() {
        return byId.stream().map(Holder.Reference::value).iterator();
    }

    private final class HolderIds implements IdMap<Holder<T>> {
        @Override
        public int getId(Holder<T> holder) {
            return holder instanceof Holder.Reference<T> reference ? byId.indexOf(reference) : -1;
        }

        @Override
        public Holder<T> byId(int id) {
            return id >= 0 && id < byId.size() ? byId.get(id) : null;
        }

        @Override
        public int size() {
            return byId.size();
        }

        @Override
        public Iterator<Holder<T>> iterator() {
            return byId.stream().<Holder<T>>map(reference -> reference).iterator();
        }
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
import com.mojang.serialization.Lifecycle;
import java.util.function.BiConsumer;
import net.minecraft.core.component.DataComponentLookup;
import net.minecraft.tags.TagLoader;
import net.minecraft.util.RandomSource;

public class MappedRegistry<T> implements WritableRegistry<T> {
    private final ResourceKey<? extends Registry<T>> key;
    private final List<Holder.Reference<T>> byId = new ArrayList<>();
    private final Map<Identifier, Holder.Reference<T>> byLocation = new LinkedHashMap<>();
    private final Map<ResourceKey<T>, Holder.Reference<T>> byKey = new LinkedHashMap<>();
    private final Map<T, Holder.Reference<T>> byValue = new IdentityHashMap<>();
    private final Map<TagKey<T>, HolderSet.Named<T>> tags = new LinkedHashMap<>();
    private final IdMap<Holder<T>> holderIds = new HolderIds();
    private Runnable tagLoader;
    private Lifecycle lifecycle = Lifecycle.stable();
    private boolean frozen;

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
            if (!(holder instanceof Holder.Reference<T> reference)) {
                return -1;
            }
            int index = byId.indexOf(reference);
            if (index >= 0) {
                return index;
            }
            Holder.Reference<T> registered = byValue.get(reference.value());
            return registered == null ? -1 : byId.indexOf(registered);
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
    public MappedRegistry(ResourceKey<? extends Registry<T>> key, Lifecycle lifecycle) {
        this.key = key;
        this.lifecycle = lifecycle;
    }
    public MappedRegistry(ResourceKey<? extends Registry<T>> key, Lifecycle initialLifecycle, boolean intrusiveHolders) {
        this.key = key;
        this.lifecycle = initialLifecycle;
    }
    public String toString() {
        return "Registry[" + key + " (" + lifecycle + ")]";
    }
    public Holder.Reference<T> register(ResourceKey<T> key, T value, RegistrationInfo registrationInfo) {
        return register(key, value);
    }
    public Optional<Holder.Reference<T>> getAny() {
        return byId.isEmpty() ? Optional.empty() : Optional.of(byId.get(0));
    }
    public Optional<RegistrationInfo> registrationInfo(ResourceKey<T> element) {
        return Optional.empty();
    }
    public Lifecycle registryLifecycle() {
        return lifecycle;
    }
    public Set<Map.Entry<ResourceKey<T>, T>> entrySet() {
        Set<Map.Entry<ResourceKey<T>, T>> entries = new java.util.LinkedHashSet<>();
        for (Holder.Reference<T> holder : byId) {
            entries.add(Map.entry(holder.key(), holder.value()));
        }
        return Collections.unmodifiableSet(entries);
    }
    public boolean isEmpty() {
        return byId.isEmpty();
    }
    public Optional<Holder.Reference<T>> getRandom(RandomSource random) {
        return byId.isEmpty() ? Optional.empty() : Optional.of(byId.get(random.nextInt(byId.size())));
    }
    public boolean containsKey(ResourceKey<T> key) {
        return byKey.containsKey(key);
    }
    public DataComponentLookup<T> componentLookup() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.MappedRegistry#componentLookup");
    }
    public Registry<T> freeze() {
        frozen = true;
        return this;
    }
    public Holder.Reference<T> createIntrusiveHolder(T value) {
        return Holder.Reference.createIntrusive(this, value);
    }
    public void bindTags(Map<TagKey<T>, List<Holder<T>>> pendingTags) {
        pendingTags.forEach(this::bindTag);
    }
    public void bindAllTagsToEmpty() {
        tags.values().forEach(set -> set.bind(List.of()));
    }
    public HolderGetter<T> createRegistrationLookup() {
        return this;
    }
    public Registry.PendingTags<T> prepareTagReload(TagLoader.LoadResult<T> tags) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.core.MappedRegistry#prepareTagReload");
    }
    interface TagSet<T> {
        static <T> TagSet<T> unbound() {
        return fromMap(new LinkedHashMap<>());
        }
        static <T> TagSet<T> fromMap(Map<TagKey<T>, HolderSet.Named<T>> tags) {
        return new TagSet<>() {
            @Override
            public boolean isBound() {
        return true;
            }

            @Override
            public Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return Optional.ofNullable(tags.get(id));
            }

            @Override
            public void forEach(BiConsumer<? super TagKey<T>, ? super HolderSet.Named<T>> action) {
        tags.forEach(action);
            }

            @Override
            public Stream<HolderSet.Named<T>> getTags() {
        return tags.values().stream();
            }
        };
        }
        boolean isBound();
        Optional<HolderSet.Named<T>> get(TagKey<T> id);
        void forEach(BiConsumer<? super TagKey<T>, ? super HolderSet.Named<T>> action);
        Stream<HolderSet.Named<T>> getTags();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries;
import com.mojang.serialization.Lifecycle;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
public class BootstrapRegistry<T> implements HolderLookup.RegistryLookup<T> {
    private final ResourceKey<? extends Registry<T>> registryKey;
    private final Lifecycle lifecycle;
    private Storage<T> storage = new RegistrationStorage();
    public BootstrapRegistry(ResourceKey<? extends Registry<T>> key, Lifecycle lifecycle) {
        this.registryKey = key;
        this.lifecycle = lifecycle;
    }
    public ResourceKey<? extends Registry<? extends T>> key() {
        return registryKey;
    }
    public Lifecycle registryLifecycle() {
        return lifecycle;
    }
    public void freeze() {
        storage = storage.freeze();
    }
    public void removeIf(Predicate<Holder.Reference<T>> predicate) {
        storage.removeIf(predicate);
    }
    public Optional<Holder.Reference<T>> get(ResourceKey<T> id) {
        return Optional.ofNullable(storage.get(id));
    }
    public Stream<Holder.Reference<T>> listElements() {
        return storage.listElements().stream();
    }
    public Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return Optional.ofNullable(storage.get(id));
    }
    public Stream<HolderSet.Named<T>> listTags() {
        return storage.listTags().stream();
    }
    class RegistrationStorage implements Storage<T> {
        private final Map<ResourceKey<T>, Holder.Reference<T>> holders = new HashMap<>();
        private final Map<TagKey<T>, HolderSet.Named<T>> sets = new HashMap<>();
        private RegistrationStorage() {
        }
        public Storage<T> freeze() {
            return new FrozenStorage<>(holders, sets);
        }
        public void removeIf(Predicate<Holder.Reference<T>> predicate) {
            holders.values().removeIf(predicate);
        }
        public Holder.Reference<T> get(ResourceKey<T> id) {
            return holders.computeIfAbsent(id, k -> Holder.Reference.createStandAlone(BootstrapRegistry.this, k));
        }
        public Collection<Holder.Reference<T>> listElements() {
            throw new UnsupportedOperationException("List is not available during bootstrap");
        }
        public HolderSet.Named<T> get(TagKey<T> id) {
            return sets.computeIfAbsent(id, k -> HolderSet.emptyNamed(BootstrapRegistry.this, k));
        }
        public Collection<HolderSet.Named<T>> listTags() {
            throw new UnsupportedOperationException("List is not available during bootstrap");
        }
    }
    static class FrozenStorage<T> implements Storage<T> {
        private final Map<ResourceKey<T>, Holder.Reference<T>> holders;
        private final Map<TagKey<T>, HolderSet.Named<T>> sets;
        private FrozenStorage(Map<ResourceKey<T>, Holder.Reference<T>> holders, Map<TagKey<T>, HolderSet.Named<T>> holderSets) {
            this.holders = Map.copyOf(holders);
            this.sets = Map.copyOf(holderSets);
        }
        public Storage<T> freeze() {
            return this;
        }
        public void removeIf(Predicate<Holder.Reference<T>> predicate) {
            throw new UnsupportedOperationException("Registry is already frozen");
        }
        public Holder.Reference<T> get(ResourceKey<T> id) {
            return holders.get(id);
        }
        public Collection<Holder.Reference<T>> listElements() {
            return holders.values();
        }
        public HolderSet.Named<T> get(TagKey<T> id) {
            return sets.get(id);
        }
        public Collection<HolderSet.Named<T>> listTags() {
            return sets.values();
        }
    }
    interface Storage<T> {
        Storage<T> freeze();
        void removeIf(Predicate<Holder.Reference<T>> predicate);
        Holder.Reference<T> get(ResourceKey<T> id);
        Collection<Holder.Reference<T>> listElements();
        HolderSet.Named<T> get(TagKey<T> id);
        Collection<HolderSet.Named<T>> listTags();
    }
}

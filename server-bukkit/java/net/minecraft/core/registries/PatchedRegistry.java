// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries;
import com.mojang.serialization.Lifecycle;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.Supplier;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.core.Cloner;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
public class PatchedRegistry<T> implements HolderLookup.RegistryLookup<T> {
    private final ResourceKey<? extends Registry<? extends T>> registryKey;
    private final Lifecycle lifecycle;
    private final Map<ResourceKey<T>, Holder.Reference<T>> entries = new HashMap<>();
    private PatchedRegistry(ResourceKey<? extends Registry<? extends T>> key, Lifecycle lifecycle) {
        this.registryKey = key;
        this.lifecycle = lifecycle;
    }
    public ResourceKey<? extends Registry<? extends T>> key() {
        return registryKey;
    }
    public Lifecycle registryLifecycle() {
        return lifecycle;
    }
    public Stream<Holder.Reference<T>> listElements() {
        return entries.values().stream();
    }
    public Optional<Holder.Reference<T>> get(ResourceKey<T> id) {
        return Optional.ofNullable(entries.get(id));
    }
    public Stream<HolderSet.Named<T>> listTags() {
        throw new UnsupportedOperationException("Tags cloning is not supported");
    }
    public Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return Optional.of(HolderSet.emptyNamed(this, id));
    }
    private static <T> HolderLookup.RegistryLookup<T> lazyMerge(Cloner.Factory factory, ResourceKey<? extends Registry<? extends T>> registryKey,
            HolderLookup.Provider base, HolderLookup.Provider patch, org.apache.commons.lang3.mutable.MutableObject<HolderLookup.Provider> finalProvider) {
        Cloner<T> cloner = factory.cloner(registryKey);
        if (cloner == null) {
            throw new NullPointerException("No cloner for " + registryKey.identifier());
        }
        HolderLookup.RegistryLookup<T> patchSide = patch.lookupOrThrow(registryKey);
        HolderLookup.RegistryLookup<T> baseSide = base.lookupOrThrow(registryKey);
        PatchedRegistry<T> merged = new PatchedRegistry<>(registryKey, patchSide.registryLifecycle().add(baseSide.registryLifecycle()));
        patchSide.listElements().forEach(src -> {
            LazyHolder<T> lazy = new LazyHolder<>(merged, src.key());
            lazy.supplier = () -> cloner.clone(src.value(), patch, finalProvider.get());
            merged.entries.put(src.key(), lazy);
        });
        baseSide.listElements().forEach(src -> merged.entries.computeIfAbsent(src.key(), k -> {
            LazyHolder<T> lazy = new LazyHolder<>(merged, k);
            lazy.supplier = () -> cloner.clone(src.value(), base, finalProvider.get());
            return lazy;
        }));
        return merged;
    }
    public static HolderLookup.Provider applyPatches(HolderLookup.Provider context, HolderLookup.Provider baseRegistries, HolderLookup.Provider patchRegistries,
            Cloner.Factory clonerFactory, Set<ResourceKey<? extends Registry<?>>> registriesToClone) {
        org.apache.commons.lang3.mutable.MutableObject<HolderLookup.Provider> box = new org.apache.commons.lang3.mutable.MutableObject<>();
        List<HolderLookup.RegistryLookup<?>> lazies = registriesToClone.stream()
                .<HolderLookup.RegistryLookup<?>>map(k -> lazyMerge(clonerFactory, k, baseRegistries, patchRegistries, box))
                .collect(Collectors.toUnmodifiableList());
        HolderLookup.Provider result = HolderLookup.Provider.create(Stream.concat(context.listRegistries(), lazies.stream()));
        box.setValue(result);
        return result;
    }
    static class LazyHolder<T> extends Holder.Reference<T> {
        private Supplier<T> supplier;
        protected LazyHolder(HolderOwner<T> owner, ResourceKey<T> key) {
            super(Holder.Reference.Type.STAND_ALONE, owner, key, null);
        }
        protected void bindValue(T value) {
            super.bindValue(value);
            supplier = null;
        }
        public T value() {
            if (supplier != null) {
                bindValue(supplier.get());
            }
            return super.value();
        }
    }
}

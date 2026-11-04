// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.mojang.serialization.DynamicOps;
import net.minecraft.core.HolderLookup;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.Registry;

public class RegistryOps<T> extends DelegatingOps<T> {
    private final HolderLookup.Provider lookupProvider;
    private RegistryInfoLookup infoLookup;

    protected RegistryOps(DynamicOps<T> delegate, HolderLookup.Provider lookupProvider) {
        super(delegate);
        this.lookupProvider = lookupProvider;
    }

    public static <T> RegistryOps<T> create(DynamicOps<T> ops, HolderLookup.Provider lookupProvider) {
        return new RegistryOps<>(ops, lookupProvider);
    }

    public HolderLookup.Provider lookupProvider() {
        return lookupProvider;
    }
    public static <T> RegistryOps<T> create(DynamicOps<T> parent, RegistryInfoLookup lookupProvider) {
        return new RegistryOps<>(parent, lookupProvider);
    }
    public static <T> Dynamic<T> injectRegistryContext(Dynamic<T> dynamic, HolderLookup.Provider lookupProvider) {
        return dynamic.convert(create(dynamic.getOps(), lookupProvider)).convert(dynamic.getOps());
    }
    private RegistryOps(DynamicOps<T> parent, RegistryInfoLookup lookupProvider) {
        super(parent);
        this.lookupProvider = null;
        this.infoLookup = lookupProvider;
    }
    public <U> RegistryOps<U> withParent(DynamicOps<U> parent) {
        return lookupProvider != null ? new RegistryOps<>(parent, lookupProvider) : new RegistryOps<>(parent, infoLookup);
    }
    public <E> Optional<HolderGetter<E>> getter(ResourceKey<? extends Registry<? extends E>> registryKey) {
        if (lookupProvider != null) {
            return lookupProvider.<E>lookup(registryKey).map(lookup -> (HolderGetter<E>) lookup);
        }
        return infoLookup.<E>lookup(registryKey);
    }
    public boolean equals(Object obj) {
        return super.equals(obj);
    }
    public int hashCode() {
        return super.hashCode();
    }
    public static <E, O> RecordCodecBuilder<O, HolderGetter<E>> retrieveGetter(ResourceKey<? extends Registry<? extends E>> registryKey) {
        return net.minecraft.util.ExtraCodecs.<HolderGetter<E>>retrieveContext(ops -> ops instanceof RegistryOps<?> registryOps
            ? registryOps.<E>getter(registryKey).map(com.mojang.serialization.DataResult::success)
                .orElseGet(() -> com.mojang.serialization.DataResult.error(() -> "Unknown registry: " + registryKey))
            : com.mojang.serialization.DataResult.error(() -> "Not a registry ops")).forGetter(owner -> null);
    }
    public static <E, O> RecordCodecBuilder<O, Holder.Reference<E>> retrieveElement(ResourceKey<E> key) {
        ResourceKey<? extends Registry<E>> registryKey = ResourceKey.createRegistryKey(key.registry());
        return net.minecraft.util.ExtraCodecs.<Holder.Reference<E>>retrieveContext(ops -> ops instanceof RegistryOps<?> registryOps
            ? registryOps.<E>getter(registryKey).flatMap(getter -> getter.get(key)).map(com.mojang.serialization.DataResult::success)
                .orElseGet(() -> com.mojang.serialization.DataResult.error(() -> "Can't find element: " + key))
            : com.mojang.serialization.DataResult.error(() -> "Not a registry ops")).forGetter(owner -> null);
    }
    static final class HolderLookupAdapter implements RegistryInfoLookup {
        private final HolderLookup.Provider lookupProvider;
        public HolderLookupAdapter(HolderLookup.Provider lookupProvider) {
            this.lookupProvider = lookupProvider;
        }
        public <E> Optional<HolderGetter<E>> lookup(ResourceKey<? extends Registry<? extends E>> registryKey) {
        return lookupProvider.<E>lookup(registryKey).map(lookup -> (HolderGetter<E>) lookup);
    }
        public boolean equals(Object obj) {
        return super.equals(obj);
        }
        public int hashCode() {
        return super.hashCode();
        }
    }
    public interface RegistryInfoLookup {
        <T> Optional<HolderGetter<T>> lookup(ResourceKey<? extends Registry<? extends T>> registryKey);
    }
}

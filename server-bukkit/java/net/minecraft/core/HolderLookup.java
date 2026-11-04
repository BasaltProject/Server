// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.serialization.DynamicOps;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
import com.mojang.serialization.Lifecycle;
import java.util.function.Predicate;
import net.minecraft.tags.TagKey;
import net.minecraft.world.flag.FeatureFlagSet;

public interface HolderLookup<T> extends HolderGetter<T> {
    Stream<Holder.Reference<T>> listElements();

    Stream<HolderSet.Named<T>> listTags();

    interface RegistryLookup<T> extends HolderLookup<T> {
        ResourceKey<? extends Registry<? extends T>> key();
            default Lifecycle registryLifecycle() {
            return Lifecycle.stable();
        }
        default RegistryLookup<T> filterFeatures(FeatureFlagSet enabledFeatures) {
        return this;
        }
        default RegistryLookup<T> filterElements(Predicate<T> filter) {
        RegistryLookup<T> parent = this;
        return new Delegate<>() {
            @Override
            public RegistryLookup<T> parent() {
        return parent;
            }

            @Override
            public Optional<Holder.Reference<T>> get(ResourceKey<T> id) {
        return parent.get(id).filter(holder -> filter.test(holder.value()));
            }

            @Override
            public Stream<Holder.Reference<T>> listElements() {
        return parent.listElements().filter(holder -> filter.test(holder.value()));
            }
        };
        }
        public interface Delegate<T> extends RegistryLookup<T> {
            RegistryLookup<T> parent();
            default ResourceKey<? extends Registry<? extends T>> key() {
        return parent().key();
            }
            default Lifecycle registryLifecycle() {
                return parent().registryLifecycle();
            }
            default Optional<Holder.Reference<T>> get(ResourceKey<T> id) {
        return parent().get(id);
            }
            default Stream<Holder.Reference<T>> listElements() {
        return parent().listElements();
            }
            default Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return parent().get(id);
            }
            default Stream<HolderSet.Named<T>> listTags() {
        return parent().listTags();
            }
            default boolean canSerializeIn(HolderOwner<T> owner) {
        return parent().canSerializeIn(owner);
            }
        }
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
            default Stream<ResourceKey<? extends Registry<?>>> listRegistryKeys() {
        return listRegistries().map(lookup -> lookup.key());
        }
        default Stream<RegistryLookup<?>> listRegistries() {
        return Stream.empty();
        }
        static Provider create(Stream<RegistryLookup<?>> lookups) {
        Map<ResourceKey<? extends Registry<?>>, RegistryLookup<?>> byKey = new java.util.LinkedHashMap<>();
        lookups.forEach(lookup -> byKey.put(lookup.key(), lookup));
        return new Provider() {
            @SuppressWarnings("unchecked")
            @Override
            public <T> Optional<? extends HolderLookup.RegistryLookup<T>> lookup(ResourceKey<? extends Registry<? extends T>> key) {
        return Optional.ofNullable((HolderLookup.RegistryLookup<T>) byKey.get(key));
            }

            @Override
            public Stream<RegistryLookup<?>> listRegistries() {
        return byKey.values().stream();
            }
        };
        }
        default Lifecycle allRegistriesLifecycle() {
        return listRegistries().map(RegistryLookup::registryLifecycle).reduce(Lifecycle.stable(), Lifecycle::add);
        }
}
    default Stream<ResourceKey<T>> listElementIds() {
        return listElements().map(Holder.Reference::key);
    }
    default Stream<TagKey<T>> listTagIds() {
        return listTags().map(HolderSet.Named::key);
    }
}

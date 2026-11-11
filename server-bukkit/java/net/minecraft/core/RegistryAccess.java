// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import java.util.Optional;
import net.minecraft.resources.ResourceKey;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;
import org.slf4j.Logger;

public interface RegistryAccess extends HolderLookup.Provider {
    @SuppressWarnings("unchecked")
    <E> Optional<Registry<E>> lookup(ResourceKey<? extends Registry<? extends E>> key);

    @Override
    @SuppressWarnings("unchecked")
    default <E> Registry<E> lookupOrThrow(ResourceKey<? extends Registry<? extends E>> key) {
        return this.<E>lookup(key).orElseThrow(() -> new IllegalStateException("Missing registry " + key));
    }

    interface Frozen extends RegistryAccess {}
    Logger LOGGER = org.slf4j.LoggerFactory.getLogger(RegistryAccess.class);
    Frozen EMPTY = new ImmutableRegistryAccess(Map.of()).freeze();
    default Stream<RegistryEntry<?>> registries() {
        return Stream.empty();
    }
    default Stream<ResourceKey<? extends Registry<?>>> listRegistryKeys() {
        return registries().map(entry -> (ResourceKey<? extends Registry<?>>) entry.key());
    }
    static Frozen fromRegistryOfRegistries(Registry<? extends Registry<?>> registries) {
        return new ImmutableRegistryAccess(registries.stream().map(registry -> (Registry<?>) registry).toList()).freeze();
    }
    default Frozen freeze() {
        RegistryAccess source = this;
        return new Frozen() {
            @Override
            public <E> Optional<Registry<E>> lookup(ResourceKey<? extends Registry<? extends E>> key) {
                return source.lookup(key);
            }

            @Override
            public Stream<RegistryEntry<?>> registries() {
                return source.registries();
            }
        };
    }
    public static record RegistryEntry<T>(ResourceKey<? extends Registry<T>> key, Registry<T> value) {
    }
    public static class ImmutableRegistryAccess implements RegistryAccess {
        private final Map<ResourceKey<?>, Registry<?>> registries;

        public ImmutableRegistryAccess(List<? extends Registry<?>> registries) {
            this.registries = new java.util.LinkedHashMap<>();
            for (Registry<?> each : registries) {
                this.registries.put(each.key(), each);
            }
        }

        public ImmutableRegistryAccess(Map<? extends ResourceKey<? extends Registry<?>>, ? extends Registry<?>> registries) {
            this.registries = new java.util.LinkedHashMap<>(registries);
        }

        public ImmutableRegistryAccess(Stream<RegistryEntry<?>> entries) {
            this.registries = new java.util.LinkedHashMap<>();
            entries.forEach(entry -> this.registries.put(entry.key(), entry.value()));
        }

        @SuppressWarnings("unchecked")
        public <E> Optional<Registry<E>> lookup(ResourceKey<? extends Registry<? extends E>> registryKey) {
            return Optional.ofNullable((Registry<E>) registries.get(registryKey));
        }

        @SuppressWarnings({"unchecked", "rawtypes"})
        public Stream<RegistryEntry<?>> registries() {
            return registries.entrySet().stream().map(entry -> (RegistryEntry<?>) new RegistryEntry((ResourceKey) entry.getKey(), entry.getValue()));
        }
    }
}

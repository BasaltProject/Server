// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import com.mojang.serialization.Lifecycle;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.core.registries.BootstrapRegistry;
import net.minecraft.core.registries.EmptyTagLookupWrapper;
import net.minecraft.core.registries.MultiRegistryBootstrap;
import net.minecraft.core.registries.PatchedRegistry;
import net.minecraft.core.registries.SingleRegistryBootstrap;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public class RegistrySetBuilder {
    private final List<RegistryStub> entries = new ArrayList<>();
    private static RegistryStub placeholder(ResourceKey<? extends Registry<?>> key) {
        return new RegistryStub() {
            public Stream<ResourceKey<? extends Registry<?>>> requiredRegistries() {
                return Stream.of(key);
            }
            public void apply(BuildState state) {
            }
        };
    }
    public <T> RegistrySetBuilder add(ResourceKey<? extends Registry<T>> key, SingleRegistryBootstrap<T> bootstrap) {
        entries.add(new RegistryStub() {
            public Stream<ResourceKey<? extends Registry<?>>> requiredRegistries() {
                return Stream.of(key);
            }
            public void apply(BuildState state) {
                bootstrap.run(state.createBootstrap(key));
            }
        });
        return this;
    }
    public RegistrySetBuilder add(MultiRegistryBootstrap bootstrap) {
        entries.add(new RegistryStub() {
            public Stream<ResourceKey<? extends Registry<?>>> requiredRegistries() {
                return bootstrap.requestedRegistries().stream();
            }
            public void apply(BuildState state) {
                bootstrap.run(state::createBootstrap);
            }
        });
        return this;
    }
    private static HolderLookup.Provider withContext(HolderLookup.Provider context, Stream<? extends HolderLookup.RegistryLookup<?>> created) {
        Map<ResourceKey<? extends Registry<?>>, HolderLookup.RegistryLookup<?>> byKey = new HashMap<>();
        context.listRegistries().forEach(r -> byKey.put(r.key(), EmptyTagLookupWrapper.wrap(r)));
        created.forEach(r -> byKey.put(r.key(), EmptyTagLookupWrapper.wrap(r)));
        return HolderLookup.Provider.create(byKey.values().stream());
    }
    public HolderLookup.Provider build(HolderLookup.Provider context) {
        BuildState state = BuildState.createAndApply(context, entries);
        List<HolderLookup.RegistryLookup<?>> built = new ArrayList<>();
        for (BootstrappedRegistryState<?> registryState : state.bootstrappedRegistries.values()) {
            registryState.bindHolders();
            registryState.freeze();
            registryState.errorOnMissingHolders(state);
            built.add(registryState.registry());
        }
        state.throwOnError();
        return withContext(context, built.stream());
    }
    private static Set<ResourceKey<? extends Registry<?>>> missingFromPatch(HolderLookup.Provider context, HolderLookup.Provider base, List<RegistryStub> stubs) {
        Set<ResourceKey<? extends Registry<?>>> known = Stream.concat(newKeys(stubs.stream()), context.listRegistryKeys()).collect(Collectors.toSet());
        return base.listRegistryKeys().filter(k -> !known.contains(k)).collect(Collectors.toSet());
    }
    public PatchedRegistries buildPatch(HolderLookup.Provider context, HolderLookup.Provider fallbackProvider, Cloner.Factory clonerFactory) {
        Set<ResourceKey<? extends Registry<?>>> missing = missingFromPatch(context, fallbackProvider, entries);
        List<RegistryStub> all = Stream.concat(entries.stream(), missing.stream().map(RegistrySetBuilder::placeholder)).toList();
        BuildState state = BuildState.createAndApply(context, all);
        List<HolderLookup.RegistryLookup<?>> built = new ArrayList<>();
        for (BootstrappedRegistryState<?> registryState : state.bootstrappedRegistries.values()) {
            registryState.bindHolders();
            registryState.validatePatchHolders(state, fallbackProvider);
            registryState.freeze();
            built.add(registryState.registry());
        }
        HolderLookup.Provider patchOnly = withContext(context, built.stream());
        state.throwOnError();
        HolderLookup.Provider full = EmptyTagLookupWrapper.wrap(
                PatchedRegistry.applyPatches(context, fallbackProvider, patchOnly, clonerFactory, state.bootstrappedRegistries.keySet()));
        return new PatchedRegistries(full, patchOnly);
    }
    @SuppressWarnings("unchecked")
    private static <T> ResourceKey<? extends Registry<T>> widen(ResourceKey<? extends Registry<? extends T>> key) {
        return (ResourceKey<? extends Registry<T>>) key;
    }
    private static Stream<ResourceKey<? extends Registry<?>>> newKeys(Stream<RegistryStub> stubs) {
        return stubs.flatMap(RegistryStub::requiredRegistries).distinct();
    }
    static record BootstrappedRegistryState<T>(BootstrapRegistry<T> registry, Map<ResourceKey<T>, T> registeredValues) {
        public static <T> BootstrappedRegistryState<T> create(ResourceKey<? extends Registry<T>> key, Lifecycle lifecycle) {
            return new BootstrappedRegistryState<>(new BootstrapRegistry<>(key, lifecycle), new HashMap<>());
        }
        public BootstrapContext<T> createBootstrapContext(BuildState state) {
            return new BootstrapContext<T>() {
                public Holder.Reference<T> register(ResourceKey<T> key, T value) {
                    T previous = registeredValues.put(key, value);
                    if (previous != null) {
                        state.errors.add(new IllegalStateException("Duplicate registration for " + key + ", new=" + value + ", old=" + previous));
                    }
                    return registry.getOrThrow(key);
                }
                public <S> HolderGetter<S> lookup(ResourceKey<? extends Registry<? extends S>> key) {
                    return state.allRegistries.lookupOrThrow(key);
                }
                public <S> Stream<Holder.Reference<S>> listContextElements(ResourceKey<? extends Registry<? extends S>> key) {
                    return state.contextRegistries.lookupOrThrow(key).listElements();
                }
            };
        }
        public void bindHolders() {
            registeredValues.forEach((key, value) -> registry.getOrThrow(key).bindValue(value));
        }
        public void freeze() {
            registry.freeze();
        }
        public void errorOnMissingHolders(BuildState state) {
            registry.listElements().forEach(holder -> {
                if (!holder.isBound()) {
                    state.errors().add(new IllegalStateException("No value registered for key " + holder.key().identifier()));
                }
            });
        }
        public void validatePatchHolders(BuildState state, HolderLookup.Provider fallback) {
            HolderLookup<T> base = fallback.lookupOrThrow(registry.key());
            registry.removeIf(holder -> {
                if (holder.isBound()) {
                    return false;
                }
                if (base.get(holder.key()).isEmpty()) {
                    state.errors().add(new IllegalStateException("Value " + holder.key().identifier() + " referenced by patched element is not present in base"));
                }
                return true;
            });
        }
    }
    static record BuildState(HolderLookup.Provider contextRegistries, HolderLookup.Provider allRegistries,
            Map<ResourceKey<? extends Registry<?>>, BootstrappedRegistryState<?>> bootstrappedRegistries, List<RuntimeException> errors) {
        public static BuildState createAndApply(HolderLookup.Provider context, List<RegistryStub> entries) {
            BuildState state = create(context, entries);
            entries.forEach(e -> e.apply(state));
            return state;
        }
        private static BuildState create(HolderLookup.Provider context, List<RegistryStub> entries) {
            Map<ResourceKey<? extends Registry<?>>, HolderLookup.RegistryLookup<?>> lookups = new LinkedHashMap<>();
            Map<ResourceKey<? extends Registry<?>>, BootstrappedRegistryState<?>> states = new LinkedHashMap<>();
            context.listRegistries().forEach(r -> lookups.put(r.key(), EmptyTagLookupWrapper.wrap(r)));
            newKeys(entries.stream()).forEach(key -> {
                BootstrappedRegistryState<?> created = BootstrappedRegistryState.create(widen(key), Lifecycle.stable());
                states.put(created.registry().key(), created);
                lookups.put(created.registry().key(), created.registry());
            });
            return new BuildState(context, HolderLookup.Provider.create(lookups.values().stream()), Map.copyOf(states), new ArrayList<>());
        }
        public void throwOnError() {
            if (errors.isEmpty()) {
                return;
            }
            IllegalStateException failure = new IllegalStateException("Errors during registry creation");
            errors.forEach(failure::addSuppressed);
            throw failure;
        }
        @SuppressWarnings("unchecked")
        public <T> BootstrapContext<T> createBootstrap(ResourceKey<? extends Registry<T>> key) {
            BootstrappedRegistryState<T> target = (BootstrappedRegistryState<T>) Objects.requireNonNull(bootstrappedRegistries.get(key), () -> "No registry named " + key.identifier());
            return target.createBootstrapContext(this);
        }
    }
    public static record PatchedRegistries(HolderLookup.Provider full, HolderLookup.Provider patches) {
    }
    interface RegistryStub {
        Stream<ResourceKey<? extends Registry<?>>> requiredRegistries();
        void apply(BuildState state);
    }
}

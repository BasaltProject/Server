// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;
import com.google.common.collect.Sets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.stream.Collectors;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
public class DataComponentInitializers {
    private final List<InitializerEntry<?>> pending = new ArrayList<>();
    public <T> void add(ResourceKey<T> key, Initializer<T> initializer) {
        pending.add(new InitializerEntry<>(key, initializer));
    }
    private Map<ResourceKey<?>, DataComponentMap.Builder> runAll(HolderLookup.Provider context) {
        Map<ResourceKey<?>, DataComponentMap.Builder> out = new HashMap<>();
        for (InitializerEntry<?> entry : pending) {
            entry.run(out.computeIfAbsent(entry.key, k -> DataComponentMap.builder()), context);
        }
        return out;
    }
    private static <T> void declare(Map<ResourceKey<? extends Registry<?>>, PendingComponentBuilders<?>> byRegistry, ResourceKey<? extends Registry<? extends T>> registryKey) {
        byRegistry.put(registryKey, new PendingComponentBuilders<T>(castKey(registryKey), new HashMap<ResourceKey<T>, DataComponentMap.Builder>()));
    }
    @SuppressWarnings("unchecked")
    private static <T> ResourceKey<? extends Registry<T>> castKey(ResourceKey<? extends Registry<? extends T>> key) {
        return (ResourceKey<? extends Registry<T>>) key;
    }
    @SuppressWarnings("unchecked")
    private static <T> void attach(Map<ResourceKey<? extends Registry<?>>, PendingComponentBuilders<?>> byRegistry, ResourceKey<T> key, DataComponentMap.Builder builder) {
        PendingComponentBuilders<T> target = (PendingComponentBuilders<T>) byRegistry.get(key.registryKey());
        target.builders.put(key, builder);
    }
    public List<PendingComponents<?>> build(HolderLookup.Provider context) {
        Map<ResourceKey<? extends Registry<?>>, PendingComponentBuilders<?>> byRegistry = new HashMap<>();
        context.listRegistryKeys().forEach(k -> declare(byRegistry, k));
        runAll(context).forEach((key, builder) -> attach(byRegistry, key, builder));
        return byRegistry.values().stream().<PendingComponents<?>>map(b -> bake(context, b)).collect(Collectors.toUnmodifiableList());
    }
    private static <T> PendingComponents<T> bake(HolderLookup.Provider context, PendingComponentBuilders<T> group) {
        List<BakedEntry<T>> baked = new ArrayList<>();
        ResourceKey<? extends Registry<T>> registryKey = group.registryKey;
        HolderLookup.RegistryLookup<T> registry = context.lookupOrThrow(registryKey);
        Set<Holder.Reference<T>> covered = Sets.newIdentityHashSet();
        group.builders.forEach((key, builder) -> {
            Holder.Reference<T> holder = registry.getOrThrow(key);
            baked.add(new BakedEntry<>(holder, builder.build()));
            covered.add(holder);
        });
        registry.listElements().filter(h -> !covered.contains(h)).forEach(h -> baked.add(new BakedEntry<>(h, DataComponentMap.EMPTY)));
        return new PendingComponents<T>() {
            public ResourceKey<? extends Registry<? extends T>> key() {
                return registryKey;
            }
            public void forEach(BiConsumer<Holder.Reference<T>, DataComponentMap> output) {
                baked.forEach(e -> output.accept(e.element, e.components));
            }
            public void apply() {
                baked.forEach(BakedEntry::apply);
            }
        };
    }
    static record InitializerEntry<T>(ResourceKey<T> key, Initializer<T> initializer) {
        public void run(DataComponentMap.Builder components, HolderLookup.Provider context) {
            initializer.run(components, context, key);
        }
    }
    static record BakedEntry<T>(Holder.Reference<T> element, DataComponentMap components) {
        public void apply() {
            element.bindComponents(components);
        }
    }
    public interface Initializer<T> {
        void run(DataComponentMap.Builder components, HolderLookup.Provider context, ResourceKey<T> key);
        default Initializer<T> andThen(Initializer<T> other) {
            return (components, context, key) -> {
                run(components, context, key);
                other.run(components, context, key);
            };
        }
        default <C> Initializer<T> add(DataComponentType<C> type, C value) {
            return andThen((components, context, key) -> components.set(type, value));
        }
    }
    public interface SingleComponentInitializer<C> {
        C create(HolderLookup.Provider context);
        default <T> Initializer<T> asInitializer(DataComponentType<C> type) {
            return (components, context, key) -> components.set(type, create(context));
        }
    }
    static record PendingComponentBuilders<T>(ResourceKey<? extends Registry<T>> registryKey, Map<ResourceKey<T>, DataComponentMap.Builder> builders) {
    }
    public interface PendingComponents<T> {
        ResourceKey<? extends Registry<? extends T>> key();
        void forEach(BiConsumer<Holder.Reference<T>, DataComponentMap> output);
        void apply();
    }
}

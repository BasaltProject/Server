// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.google.common.collect.ImmutableMultimap;
import com.google.common.collect.Multimap;
import java.util.Collection;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;

public class DataComponentLookup<T> {
    private final Iterable<? extends Holder<T>> elements;
    private final Map<DataComponentType<?>, ComponentStorage<?, T>> storages = new ConcurrentHashMap<>();

    public DataComponentLookup(Iterable<? extends Holder<T>> elements) {
        this.elements = elements;
    }

    @SuppressWarnings("unchecked")
    private <C> ComponentStorage<C, T> storage(DataComponentType<C> type) {
        return (ComponentStorage<C, T>) storages.computeIfAbsent(type, key -> scan((DataComponentType<C>) key));
    }

    private <C> ComponentStorage<C, T> scan(DataComponentType<C> type) {
        ImmutableMultimap.Builder<C, Holder<T>> found = ImmutableMultimap.builder();
        for (Holder<T> element : elements) {
            C value = element.components().get(type);
            if (value != null) {
                found.put(value, element);
            }
        }
        return new ComponentStorage<>(found.build());
    }

    public <C> Stream<Holder<T>> findMatching(DataComponentType<C> type, Predicate<C> predicate) {
        return storage(type).findMatching(predicate);
    }

    public <C> Collection<Holder<T>> findAll(DataComponentType<C> type, C value) {
        return storage(type).findAll(value);
    }

    public <C> Collection<Holder<T>> findAll(DataComponentType<C> type) {
        return storage(type).valueToComponent.values();
    }

    static record ComponentStorage<C, T>(Multimap<C, Holder<T>> valueToComponent) {
        public Collection<Holder<T>> findAll(C value) {
            return valueToComponent.get(value);
        }

        public Stream<Holder<T>> findMatching(Predicate<C> predicate) {
            return valueToComponent.asMap().entrySet().stream().filter(entry -> predicate.test(entry.getKey()))
                .flatMap(entry -> entry.getValue().stream());
        }
    }
}

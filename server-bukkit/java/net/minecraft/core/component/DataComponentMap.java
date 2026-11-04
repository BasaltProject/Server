// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.mojang.serialization.Codec;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.Stream;

public interface DataComponentMap extends Iterable<TypedDataComponent<?>>, DataComponentGetter {
    DataComponentMap EMPTY = new Builder.SimpleMap(Map.of());
    Codec<DataComponentMap> CODEC = makeCodec(DataComponentType.CODEC);

    static Codec<DataComponentMap> makeCodec(Codec<DataComponentType<?>> componentTypeCodec) {
        return makeCodecFromMap(org.basaltmc.bukkit.facade.ComponentValueCodecs.valueMap(componentTypeCodec));
    }

    static Codec<DataComponentMap> makeCodecFromMap(Codec<Map<DataComponentType<?>, Object>> mapCodec) {
        return mapCodec.flatXmap(map -> {
            Builder builder = builder();
            map.forEach((type, value) -> builder.setUnchecked((DataComponentType) type, value));
            return com.mojang.serialization.DataResult.success(builder.build());
        }, components -> {
            Map<DataComponentType<?>, Object> out = new LinkedHashMap<>();
            for (TypedDataComponent<?> entry : components) {
                if (!entry.type().isTransient()) {
                    out.put(entry.type(), entry.value());
                }
            }
            return com.mojang.serialization.DataResult.success(out);
        });
    }

    static DataComponentMap composite(DataComponentMap prototype, DataComponentMap overrides) {
        Map<DataComponentType<?>, Object> merged = new LinkedHashMap<>();
        for (TypedDataComponent<?> entry : prototype) {
            merged.put(entry.type(), entry.value());
        }
        for (TypedDataComponent<?> entry : overrides) {
            merged.put(entry.type(), entry.value());
        }
        return new Builder.SimpleMap(merged);
    }

    static Builder builder() {
        return new Builder();
    }

    Set<DataComponentType<?>> keySet();

    default boolean has(DataComponentType<?> type) {
        return get(type) != null;
    }

    @Override
    default Iterator<TypedDataComponent<?>> iterator() {
        return stream().iterator();
    }

    default Stream<TypedDataComponent<?>> stream() {
        return keySet().stream().map(type -> TypedDataComponent.createUnchecked((DataComponentType) type, get(type)));
    }

    default int size() {
        return keySet().size();
    }

    default boolean isEmpty() {
        return size() == 0;
    }

    default DataComponentMap filter(Predicate<DataComponentType<?>> predicate) {
        Map<DataComponentType<?>, Object> kept = new LinkedHashMap<>();
        for (DataComponentType<?> type : keySet()) {
            if (predicate.test(type)) {
                kept.put(type, get(type));
            }
        }
        return new Builder.SimpleMap(kept);
    }

    public static class Builder {
        private final Map<DataComponentType<?>, Object> map = new LinkedHashMap<>();
        private final java.util.List<Consumer<DataComponentMap>> validators = new java.util.ArrayList<>();

        private Builder() {
        }

        public <T> Builder set(DataComponentType<T> type, T value) {
            return setUnchecked(type, value);
        }

        <T> Builder setUnchecked(DataComponentType<T> type, Object value) {
            if (value == null) {
                map.remove(type);
            } else {
                map.put(type, value);
            }
            return this;
        }

        public Builder addAll(DataComponentMap other) {
            for (TypedDataComponent<?> entry : other) {
                map.put(entry.type(), entry.value());
            }
            return this;
        }

        public Builder addValidator(Consumer<DataComponentMap> newValidator) {
            validators.add(newValidator);
            return this;
        }

        public DataComponentMap build() {
            DataComponentMap built = new SimpleMap(new LinkedHashMap<>(map));
            validators.forEach(validator -> validator.accept(built));
            return built.isEmpty() ? EMPTY : built;
        }

        static record SimpleMap(Map<DataComponentType<?>, Object> map) implements DataComponentMap {
            @Override
            @SuppressWarnings("unchecked")
            public <T> T get(DataComponentType<? extends T> type) {
                return (T) map.get(type);
            }

            @Override
            public boolean has(DataComponentType<?> type) {
                return map.containsKey(type);
            }

            @Override
            public Set<DataComponentType<?>> keySet() {
                return map.keySet();
            }

            @Override
            public int size() {
                return map.size();
            }

            @Override
            public String toString() {
                return map.toString();
            }
        }
    }
}

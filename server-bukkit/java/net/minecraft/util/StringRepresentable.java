// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.Keyable;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.function.ToIntFunction;
public interface StringRepresentable {
    public default String getSerializedName() {
        throw Unsupported.hit("net.minecraft.util.StringRepresentable.getSerializedName");
    }
    int PRE_BUILT_MAP_THRESHOLD = 16;

    private static int ordinalOf(StringRepresentable value) {
        return value instanceof Enum<?> e ? e.ordinal() : 0;
    }

    static <E extends Enum<E> & StringRepresentable> EnumCodec<E> fromEnum(Supplier<E[]> values) {
        return fromEnumWithMapping(values, name -> name);
    }
    static <E extends Enum<E> & StringRepresentable> EnumCodec<E> fromEnumWithMapping(Supplier<E[]> values, Function<String, String> converter) {
        E[] all = values.get();
        Function<String, E> lookup = createNameLookup(all, constant -> converter.apply(constant.getSerializedName()));
        return new EnumCodec<>(all, lookup);
    }
    static <T extends StringRepresentable> Codec<T> fromValues(Supplier<T[]> values) {
        T[] all = values.get();
        Function<String, T> lookup = createNameLookup(all);
        return new StringRepresentableCodec<>(all, lookup, StringRepresentable::ordinalOf);
    }
    static <T extends StringRepresentable> Function<String, T> createNameLookup(T[] valueArray) {
        return createNameLookup(valueArray, StringRepresentable::getSerializedName);
    }
    static <T> Function<String, T> createNameLookup(T[] valueArray, Function<T, String> converter) {
        java.util.Map<String, T> names = new java.util.HashMap<>();
        for (T value : valueArray) {
            String key = converter.apply(value);
            if (names.put(key, value) != null) {
                throw new IllegalArgumentException("Duplicate string representation " + key);
            }
        }
        return names::get;
    }
    static Keyable keys(StringRepresentable[] values) {
        return new Keyable() {
            public <T> java.util.stream.Stream<T> keys(DynamicOps<T> ops) {
                return java.util.Arrays.stream(values).map(StringRepresentable::getSerializedName).map(ops::createString);
            }
        };
    }
    public static class StringRepresentableCodec<S extends StringRepresentable> implements Codec<S> {
        private final S[] values;
        private final Function<String, S> names;
        private final ToIntFunction<S> ids;

        public StringRepresentableCodec(S[] valueArray, Function<String, S> nameResolver, ToIntFunction<S> idResolver) {
            this.values = valueArray;
            this.names = nameResolver;
            this.ids = idResolver;
        }
        public <T> DataResult<Pair<S, T>> decode(DynamicOps<T> ops, T input) {
            if (ops.compressMaps()) {
                return ops.getNumberValue(input).flatMap(id -> {
                    int index = id.intValue();
                    return index >= 0 && index < values.length ? DataResult.success(values[index])
                        : DataResult.error(() -> "Unknown element id: " + id);
                }).map(value -> Pair.of(value, ops.empty()));
            }
            return ops.getStringValue(input).flatMap(name -> {
                S found = names.apply(name);
                return found == null ? DataResult.error(() -> "Unknown element name: " + name)
                    : DataResult.success(found);
            }).map(value -> Pair.of(value, ops.empty()));
        }
        public <T> DataResult<T> encode(S input, DynamicOps<T> ops, T prefix) {
            return ops.compressMaps() ? ops.mergeToPrimitive(prefix, ops.createInt(ids.applyAsInt(input)))
                : ops.mergeToPrimitive(prefix, ops.createString(input.getSerializedName()));
        }
    }
    public static class EnumCodec<E extends Enum<E> & StringRepresentable> extends StringRepresentableCodec<E> {
        private final Function<String, E> lookup;

        public EnumCodec(E[] valueArray, Function<String, E> nameResolver) {
            super(valueArray, nameResolver, constant -> constant.ordinal());
            this.lookup = nameResolver;
        }
        public E byName(String name) {
            return lookup.apply(name);
        }
        public E byName(String name, E _default) {
            E found = lookup.apply(name);
            return found != null ? found : _default;
        }
        public E byName(String name, Supplier<? extends E> defaultSupplier) {
            E found = lookup.apply(name);
            return found != null ? found : defaultSupplier.get();
        }
    }
}

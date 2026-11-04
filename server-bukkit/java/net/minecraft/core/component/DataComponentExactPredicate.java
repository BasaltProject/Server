// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Predicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public final class DataComponentExactPredicate implements Predicate<DataComponentGetter> {
    public static final Codec<DataComponentExactPredicate> CODEC = DataComponentType.VALUE_MAP_CODEC.flatXmap(map -> {
        List<TypedDataComponent<?>> entries = new ArrayList<>();
        map.forEach((type, value) -> entries.add(TypedDataComponent.createUnchecked((DataComponentType) type, value)));
        return com.mojang.serialization.DataResult.success(new DataComponentExactPredicate(entries));
    }, predicate -> {
        java.util.Map<DataComponentType<?>, Object> out = new java.util.LinkedHashMap<>();
        for (TypedDataComponent<?> entry : predicate.expectedComponents) {
            out.put(entry.type(), entry.value());
        }
        return com.mojang.serialization.DataResult.success(out);
    });
    public static final StreamCodec<RegistryFriendlyByteBuf, DataComponentExactPredicate> STREAM_CODEC = TypedDataComponent.STREAM_CODEC
        .apply(net.minecraft.network.codec.ByteBufCodecs.list()).map(DataComponentExactPredicate::new, predicate -> predicate.expectedComponents);
    public static final DataComponentExactPredicate EMPTY = new DataComponentExactPredicate(List.of());

    private final List<TypedDataComponent<?>> expectedComponents;

    private DataComponentExactPredicate(List<TypedDataComponent<?>> expectedComponents) {
        this.expectedComponents = expectedComponents;
    }

    public static Builder builder() {
        return new Builder();
    }

    public static <T> DataComponentExactPredicate expect(DataComponentType<T> type, T value) {
        return new DataComponentExactPredicate(List.of(new TypedDataComponent<>(type, value)));
    }

    public static DataComponentExactPredicate allOf(DataComponentMap components) {
        List<TypedDataComponent<?>> all = new ArrayList<>();
        components.forEach(all::add);
        return new DataComponentExactPredicate(all);
    }

    public static DataComponentExactPredicate someOf(DataComponentMap components, DataComponentType<?>... types) {
        List<TypedDataComponent<?>> some = new ArrayList<>();
        for (DataComponentType<?> type : types) {
            TypedDataComponent<?> found = components.getTyped(type);
            if (found != null) {
                some.add(found);
            }
        }
        return new DataComponentExactPredicate(some);
    }

    public boolean isEmpty() {
        return expectedComponents.isEmpty();
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof DataComponentExactPredicate other && expectedComponents.equals(other.expectedComponents);
    }

    @Override
    public int hashCode() {
        return expectedComponents.hashCode();
    }

    @Override
    public String toString() {
        return expectedComponents.toString();
    }

    @Override
    public boolean test(DataComponentGetter actualComponents) {
        for (TypedDataComponent<?> expected : expectedComponents) {
            if (!Objects.equals(expected.value(), actualComponents.get(expected.type()))) {
                return false;
            }
        }
        return true;
    }

    public boolean alwaysMatches() {
        return expectedComponents.isEmpty();
    }

    public DataComponentPatch asPatch() {
        DataComponentPatch.Builder builder = DataComponentPatch.builder();
        for (TypedDataComponent<?> entry : expectedComponents) {
            builder.set(entry);
        }
        return builder.build();
    }

    public static class Builder {
        private final List<TypedDataComponent<?>> expected = new ArrayList<>();

        private Builder() {}

        public <T> Builder expect(TypedDataComponent<T> value) {
            expected.add(value);
            return this;
        }

        public <T> Builder expect(DataComponentType<? super T> type, T value) {
            expected.add(new TypedDataComponent<>((DataComponentType) type, value));
            return this;
        }

        public DataComponentExactPredicate build() {
            return expected.isEmpty() ? EMPTY : new DataComponentExactPredicate(List.copyOf(expected));
        }
    }
}

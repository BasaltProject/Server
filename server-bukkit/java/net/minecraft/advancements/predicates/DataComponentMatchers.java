// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.Predicate;
import net.minecraft.core.component.DataComponentExactPredicate;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.predicates.DataComponentPredicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

public record DataComponentMatchers(DataComponentExactPredicate exact, Map<DataComponentPredicate.Type<?>, DataComponentPredicate> partial) implements Predicate<DataComponentGetter> {
    public static final DataComponentMatchers ANY = new DataComponentMatchers(DataComponentExactPredicate.EMPTY, Map.of());
    public static final MapCodec<DataComponentMatchers> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        DataComponentExactPredicate.CODEC.optionalFieldOf("components", DataComponentExactPredicate.EMPTY).forGetter(DataComponentMatchers::exact),
        DataComponentPredicate.CODEC.optionalFieldOf("predicates", Map.of()).forGetter(DataComponentMatchers::partial)).apply(i, DataComponentMatchers::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, DataComponentMatchers> STREAM_CODEC = StreamCodec.composite(
        DataComponentExactPredicate.STREAM_CODEC, DataComponentMatchers::exact,
        DataComponentPredicate.STREAM_CODEC, DataComponentMatchers::partial, DataComponentMatchers::new);

    @Override
    public boolean test(DataComponentGetter values) {
        if (!exact.test(values)) {
            return false;
        }
        for (DataComponentPredicate predicate : partial.values()) {
            if (!predicate.matches(values)) {
                return false;
            }
        }
        return true;
    }

    public boolean isEmpty() {
        return exact.isEmpty() && partial.isEmpty();
    }

    public static class Builder {
        private DataComponentExactPredicate exact = DataComponentExactPredicate.EMPTY;
        private final Map<DataComponentPredicate.Type<?>, DataComponentPredicate> partial = new LinkedHashMap<>();

        private Builder() {}

        public static Builder components() {
            return new Builder();
        }

        public <T extends DataComponentType<?>> Builder any(DataComponentType<?> type) {
            DataComponentPredicate.AnyValueType wrapper = DataComponentPredicate.AnyValueType.create(type);
            partial.put(wrapper, wrapper.predicate());
            return this;
        }

        public <T extends DataComponentPredicate> Builder partial(DataComponentPredicate.Type<T> type, T predicate) {
            partial.put(type, predicate);
            return this;
        }

        public Builder exact(DataComponentExactPredicate exact) {
            this.exact = exact;
            return this;
        }

        public DataComponentMatchers build() {
            return new DataComponentMatchers(exact, Map.copyOf(partial));
        }
    }
}

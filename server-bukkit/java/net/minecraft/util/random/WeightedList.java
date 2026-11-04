// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.random;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.RandomSource;

public final class WeightedList<E> {
    private final int totalWeight;
    private final List<Weighted<E>> items;

    private WeightedList(List<? extends Weighted<E>> items) {
        this.items = List.copyOf(items);
        long total = 0L;
        for (Weighted<E> item : this.items) {
            total += item.weight();
        }
        if (total > Integer.MAX_VALUE) {
            throw new IllegalStateException("Sum of weights must be <= 2147483647");
        }
        this.totalWeight = (int) total;
    }

    public static <E> WeightedList<E> of() {
        return new WeightedList<>(List.of());
    }

    public static <E> WeightedList<E> of(E value) {
        return new WeightedList<>(List.of(new Weighted<>(value, 1)));
    }

    @SafeVarargs
    public static <E> WeightedList<E> of(E... items) {
        List<Weighted<E>> weighted = new ArrayList<>(items.length);
        for (E item : items) {
            weighted.add(new Weighted<>(item, 1));
        }
        return new WeightedList<>(weighted);
    }

    @SafeVarargs
    public static <E> WeightedList<E> of(Weighted<E>... items) {
        return new WeightedList<>(List.of(items));
    }

    public static <E> WeightedList<E> of(List<Weighted<E>> items) {
        return new WeightedList<>(items);
    }

    public static <E> Builder<E> builder() {
        return new Builder<>();
    }

    public boolean isEmpty() {
        return items.isEmpty();
    }

    public <T> WeightedList<T> map(Function<E, T> mapper) {
        return new WeightedList<>(items.stream().map(item -> item.map(mapper)).toList());
    }

    public Optional<E> getRandom(RandomSource random) {
        if (totalWeight == 0) {
            return Optional.empty();
        }
        int pick = random.nextInt(totalWeight);
        for (Weighted<E> item : items) {
            pick -= item.weight();
            if (pick < 0) {
                return Optional.of(item.value());
            }
        }
        return Optional.empty();
    }

    public E getRandomOrThrow(RandomSource random) {
        return getRandom(random).orElseThrow(() -> new IllegalStateException("Weighted list has no elements"));
    }

    public List<Weighted<E>> unwrap() {
        return items;
    }

    public static <E> Codec<WeightedList<E>> codec(Codec<E> elementCodec) {
        return Weighted.codec(elementCodec).listOf().xmap(WeightedList::of, WeightedList::unwrap);
    }

    public static <E> Codec<WeightedList<E>> codec(MapCodec<E> elementCodec) {
        return Weighted.codec(elementCodec).listOf().xmap(WeightedList::of, WeightedList::unwrap);
    }

    public static <E> Codec<WeightedList<E>> nonEmptyCodec(Codec<E> elementCodec) {
        return nonEmpty(codec(elementCodec));
    }

    public static <E> Codec<WeightedList<E>> nonEmptyCodec(MapCodec<E> elementCodec) {
        return nonEmpty(codec(elementCodec));
    }

    private static <E> Codec<WeightedList<E>> nonEmpty(Codec<WeightedList<E>> codec) {
        return codec.validate(list -> list.isEmpty() ? DataResult.error(() -> "Weighted list must have at least one element")
            : DataResult.success(list));
    }

    public static <E, B extends ByteBuf> StreamCodec<B, WeightedList<E>> streamCodec(StreamCodec<B, E> elementCodec) {
        return Weighted.streamCodec(elementCodec).apply(ByteBufCodecs.list()).map(WeightedList::of, WeightedList::unwrap);
    }

    public boolean contains(E value) {
        for (Weighted<E> item : items) {
            if (item.value().equals(value)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean equals(Object obj) {
        return this == obj || obj instanceof WeightedList<?> other && totalWeight == other.totalWeight && items.equals(other.items);
    }

    @Override
    public int hashCode() {
        return items.hashCode();
    }

    public static class Builder<E> {
        private final List<Weighted<E>> result = new ArrayList<>();

        public Builder<E> add(E item) {
            return add(item, 1);
        }

        public Builder<E> add(E item, int weight) {
            result.add(new Weighted<>(item, weight));
            return this;
        }

        public Builder<E> addAll(WeightedList<E> other) {
            result.addAll(other.unwrap());
            return this;
        }

        public WeightedList<E> build() {
            return new WeightedList<>(result);
        }
    }
}

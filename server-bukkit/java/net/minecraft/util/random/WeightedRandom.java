// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.random;

import java.util.List;
import java.util.Optional;
import java.util.function.ToIntFunction;
import net.minecraft.util.RandomSource;

public class WeightedRandom {
    private WeightedRandom() {
    }

    public static <T> int getTotalWeight(List<T> items, ToIntFunction<T> weightGetter) {
        long total = 0L;
        for (T item : items) {
            total += weightGetter.applyAsInt(item);
        }
        if (total > Integer.MAX_VALUE) {
            throw new IllegalArgumentException("Sum of weights must be <= 2147483647");
        }
        return (int) total;
    }

    public static <T> Optional<T> getRandomItem(RandomSource random, List<T> items, int totalWeight, ToIntFunction<T> weightGetter) {
        if (totalWeight < 0) {
            throw new IllegalArgumentException("Negative total weight in getRandomItem");
        }
        if (totalWeight == 0) {
            return Optional.empty();
        }
        return getWeightedItem(items, random.nextInt(totalWeight), weightGetter);
    }

    public static <T> Optional<T> getWeightedItem(List<T> items, int index, ToIntFunction<T> weightGetter) {
        int remaining = index;
        for (T item : items) {
            remaining -= weightGetter.applyAsInt(item);
            if (remaining < 0) {
                return Optional.of(item);
            }
        }
        return Optional.empty();
    }

    public static <T> Optional<T> getRandomItem(RandomSource random, List<T> items, ToIntFunction<T> weightGetter) {
        return getRandomItem(random, items, getTotalWeight(items, weightGetter), weightGetter);
    }
}

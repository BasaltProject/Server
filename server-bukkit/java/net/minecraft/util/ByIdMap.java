// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import it.unimi.dsi.fastutil.ints.Int2ObjectLinkedOpenHashMap;
import java.util.Arrays;
import java.util.function.IntFunction;
import java.util.function.ToIntFunction;

public class ByIdMap {
    public static <T> IntFunction<T> sparse(ToIntFunction<T> idGetter, T[] values, T fallback) {
        Int2ObjectLinkedOpenHashMap<T> byId = new Int2ObjectLinkedOpenHashMap<>(values.length);
        for (T value : values) {
            if (byId.put(idGetter.applyAsInt(value), value) != null) {
                throw new IllegalArgumentException("Duplicate entry on id " + idGetter.applyAsInt(value));
            }
        }
        return id -> byId.getOrDefault(id, fallback);
    }

    public static <T> IntFunction<T> continuous(ToIntFunction<T> idGetter, T[] values, OutOfBoundsStrategy strategy) {
        T[] table = Arrays.copyOf(values, values.length);
        Arrays.fill(table, null);
        for (T value : values) {
            int id = idGetter.applyAsInt(value);
            if (id < 0 || id >= table.length) {
                throw new IllegalArgumentException("Values are not continuous, found out of range id " + id);
            }
            if (table[id] != null) {
                throw new IllegalArgumentException("Duplicate entry on id " + id);
            }
            table[id] = value;
        }
        for (int i = 0; i < table.length; i++) {
            if (table[i] == null) {
                throw new IllegalArgumentException("Missing entry on id " + i);
            }
        }
        int length = table.length;
        return switch (strategy) {
            case ZERO -> id -> id >= 0 && id < length ? table[id] : table[0];
            case WRAP -> id -> table[Math.floorMod(id, length)];
            case CLAMP -> id -> table[id < 0 ? 0 : Math.min(id, length - 1)];
        };
    }

    public static enum OutOfBoundsStrategy {
        ZERO, WRAP, CLAMP;
    }
}

package net.minecraft.world.level.chunk;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.function.Predicate;

public class HashMapPalette<T> implements Palette<T> {
    private final IdentityHashMap<T, Integer> ids = new IdentityHashMap<>();
    private final ArrayList<T> values = new ArrayList<>();
    private final int bits;

    public HashMapPalette(int bits, List<T> entries) {
        this(bits);
        for (T entry : entries) {
            add(entry);
        }
    }

    public HashMapPalette(int bits) {
        this.bits = bits;
    }

    public static <A> Palette<A> create(int bits, List<A> entries) {
        return new HashMapPalette<>(bits, entries);
    }

    private int add(T value) {
        int id = values.size();
        values.add(value);
        ids.putIfAbsent(value, id);
        return id;
    }

    @Override
    public int idFor(T value, PaletteResize<T> resizeHandler) {
        Integer found = ids.get(value);
        if (found != null) {
            return found;
        }
        int id = values.size();
        if (id < (1 << bits)) {
            return add(value);
        }
        return resizeHandler.onResize(bits + 1, value);
    }

    @Override
    public boolean maybeHas(Predicate<T> predicate) {
        for (T value : values) {
            if (predicate.test(value)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public T valueFor(int index) {
        if (index >= 0 && index < values.size()) {
            return values.get(index);
        }
        throw new IllegalStateException("Missing palette entry for id " + index + ".");
    }

    @Override
    public int getSize() {
        return values.size();
    }

    public List<T> getEntries() {
        return List.copyOf(values);
    }

    @Override
    public Palette<T> copy() {
        return new HashMapPalette<>(bits, values);
    }
}

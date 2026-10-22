package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.function.Predicate;

public class LinearPalette<T> implements Palette<T> {
    private final T[] values;
    private final int bits;
    private int size;

    @SuppressWarnings("unchecked")
    private LinearPalette(int bits, List<T> entries) {
        this.values = (T[]) new Object[1 << bits];
        this.bits = bits;
        for (int i = 0; i < entries.size(); i++) {
            values[i] = entries.get(i);
        }
        this.size = entries.size();
    }

    private LinearPalette(T[] values, int bits, int size) {
        this.values = values;
        this.bits = bits;
        this.size = size;
    }

    public static <A> Palette<A> create(int bits, List<A> entries) {
        return new LinearPalette<>(bits, entries);
    }

    @Override
    public int idFor(T value, PaletteResize<T> resizeHandler) {
        for (int i = 0; i < size; i++) {
            if (values[i] == value) {
                return i;
            }
        }
        int at = size;
        if (at < values.length) {
            values[at] = value;
            size++;
            return at;
        }
        return resizeHandler.onResize(bits + 1, value);
    }

    @Override
    public boolean maybeHas(Predicate<T> predicate) {
        for (int i = 0; i < size; i++) {
            if (predicate.test(values[i])) {
                return true;
            }
        }
        return false;
    }

    @Override
    public T valueFor(int index) {
        if (index >= 0 && index < size) {
            return values[index];
        }
        throw new IllegalStateException("Missing palette entry for id " + index + ".");
    }

    @Override
    public int getSize() {
        return size;
    }

    @Override
    public Palette<T> copy() {
        return new LinearPalette<>(values.clone(), bits, size);
    }
}

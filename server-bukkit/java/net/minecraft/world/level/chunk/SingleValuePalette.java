package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.function.Predicate;

public class SingleValuePalette<T> implements Palette<T> {
    private T value;

    public SingleValuePalette(List<T> entries) {
        if (!entries.isEmpty()) {
            this.value = entries.get(0);
        }
    }

    public static <A> Palette<A> create(int bits, List<A> entries) {
        return new SingleValuePalette<>(entries);
    }

    @Override
    public int idFor(T candidate, PaletteResize<T> resizeHandler) {
        if (value == null) {
            value = candidate;
            return 0;
        }
        if (value == candidate) {
            return 0;
        }
        return resizeHandler.onResize(1, candidate);
    }

    @Override
    public boolean maybeHas(Predicate<T> predicate) {
        return predicate.test(value);
    }

    @Override
    public T valueFor(int index) {
        if (value != null && index == 0) {
            return value;
        }
        throw new IllegalStateException("Missing palette entry for id " + index + ".");
    }

    @Override
    public int getSize() {
        return 1;
    }

    @Override
    public Palette<T> copy() {
        if (value == null) {
            throw new IllegalStateException("Use of an uninitialized palette");
        }
        return this;
    }
}

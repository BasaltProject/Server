package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.function.Predicate;

public interface Palette<T> {
    int idFor(T value, PaletteResize<T> resizeHandler);

    boolean maybeHas(Predicate<T> predicate);

    T valueFor(int index);

    int getSize();

    Palette<T> copy();

    interface Factory {
        <A> Palette<A> create(int bits, List<A> paletteEntries);
    }
}

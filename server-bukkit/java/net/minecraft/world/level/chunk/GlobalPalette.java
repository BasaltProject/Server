package net.minecraft.world.level.chunk;

import java.util.function.Predicate;
import net.minecraft.core.IdMap;

public class GlobalPalette<T> implements Palette<T> {
    private final IdMap<T> registry;

    public GlobalPalette(IdMap<T> registry) {
        this.registry = registry;
    }

    @Override
    public int idFor(T value, PaletteResize<T> resizeHandler) {
        int id = registry.getId(value);
        return id == -1 ? 0 : id;
    }

    @Override
    public boolean maybeHas(Predicate<T> predicate) {
        return true;
    }

    @Override
    public T valueFor(int index) {
        T value = registry.byId(index);
        if (value == null) {
            throw new IllegalStateException("Missing palette entry for id " + index + ".");
        }
        return value;
    }

    @Override
    public int getSize() {
        return registry.size();
    }

    @Override
    public Palette<T> copy() {
        return this;
    }
}

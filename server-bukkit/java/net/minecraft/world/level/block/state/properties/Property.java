package net.minecraft.world.level.block.state.properties;

import java.util.List;
import java.util.Optional;

public abstract class Property<T extends Comparable<T>> {
    private final Class<T> clazz;
    private final String name;

    protected Property(String name, Class<T> clazz) {
        this.clazz = clazz;
        this.name = name;
    }

    public String getName() {
        return name;
    }

    public Class<T> getValueClass() {
        return clazz;
    }

    public abstract List<T> getPossibleValues();

    public abstract String getName(T value);

    public abstract Optional<T> getValue(String name);

    public abstract int getInternalIndex(T value);

    @Override
    public String toString() {
        return getClass().getSimpleName() + "{name=" + name + ", clazz=" + clazz + ", values=" + getPossibleValues() + "}";
    }
}

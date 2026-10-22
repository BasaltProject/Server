package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.Map;
import java.util.Optional;
import net.minecraft.world.level.block.state.properties.Property;

public abstract class StateHolder<O, S> {
    protected final O owner;

    protected StateHolder(O owner) {
        this.owner = owner;
    }

    public Collection<Property<?>> getProperties() {
        return java.util.List.of();
    }

    public <T extends Comparable<T>> boolean hasProperty(Property<T> property) {
        return false;
    }

    public <T extends Comparable<T>> T getValue(Property<T> property) {
        throw new IllegalArgumentException("Cannot get property " + property + " as it does not exist in " + owner);
    }

    public <T extends Comparable<T>> Optional<T> getOptionalValue(Property<T> property) {
        return hasProperty(property) ? Optional.of(getValue(property)) : Optional.empty();
    }

    public <T extends Comparable<T>, V extends T> S setValue(Property<T> property, V value) {
        throw new IllegalArgumentException("Cannot set property " + property + " as it does not exist in " + owner);
    }

    public <T extends Comparable<T>, V extends T> S trySetValue(Property<T> property, V value) {
        return hasProperty(property) ? setValue(property, value) : self();
    }

    public Map<Property<?>, Comparable<?>> getValues() {
        return Map.of();
    }

    @SuppressWarnings("unchecked")
    protected S self() {
        return (S) this;
    }
}

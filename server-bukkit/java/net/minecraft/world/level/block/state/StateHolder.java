// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.Map;
import java.util.Optional;
import net.minecraft.world.level.block.state.properties.Property;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Function;
import java.util.stream.Stream;

public abstract class StateHolder<O, S> {
    protected final O owner;
    private Property<?>[] keys = new Property<?>[0];
    private Comparable<?>[] values = new Comparable<?>[0];
    private S[][] neighbors;

    protected StateHolder(O owner) {
        this.owner = owner;
    }

    private int indexOf(Property<?> property) {
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] == property || keys[i].equals(property)) {
                return i;
            }
        }
        return -1;
    }

    public Collection<Property<?>> getProperties() {
        return java.util.List.of(keys);
    }

    public <T extends Comparable<T>> boolean hasProperty(Property<T> property) {
        return indexOf(property) >= 0;
    }

    @SuppressWarnings("unchecked")
    public <T extends Comparable<T>> T getValue(Property<T> property) {
        int index = indexOf(property);
        if (index < 0) {
            throw new IllegalArgumentException("Cannot get property " + property + " as it does not exist in " + owner);
        }
        return (T) values[index];
    }

    public <T extends Comparable<T>> Optional<T> getOptionalValue(Property<T> property) {
        return hasProperty(property) ? Optional.of(getValue(property)) : Optional.empty();
    }

    public <T extends Comparable<T>, V extends T> S setValue(Property<T> property, V value) {
        int index = indexOf(property);
        if (index < 0) {
            throw new IllegalArgumentException("Cannot set property " + property + " as it does not exist in " + owner);
        }
        if (values[index].equals(value)) {
            return self();
        }
        int slot = property.getInternalIndex(value);
        if (slot < 0 || neighbors == null) {
            throw new IllegalArgumentException("Cannot set property " + property + " to " + value + " on " + owner);
        }
        return neighbors[index][slot];
    }

    public <T extends Comparable<T>, V extends T> S trySetValue(Property<T> property, V value) {
        return hasProperty(property) ? setValue(property, value) : self();
    }

    public Map<Property<?>, Comparable<?>> getValues() {
        Map<Property<?>, Comparable<?>> all = new java.util.LinkedHashMap<>();
        for (int i = 0; i < keys.length; i++) {
            all.put(keys[i], values[i]);
        }
        return all;
    }

    @SuppressWarnings("unchecked")
    protected S self() {
        return (S) this;
    }
    public static final String ID_TAG = "Name";
    public static final String PROPERTIES_TAG = "Properties";
    protected StateHolder(O owner, Property<?>[] propertyKeys, Comparable<?>[] propertyValues) {
        this.owner = owner;
        if (propertyKeys != null && propertyValues != null) {
            this.keys = propertyKeys;
            this.values = propertyValues;
        }
    }
    public <T extends Comparable<T>> S cycle(Property<T> property) {
        return setValue(property, findNextInCollection(property.getPossibleValues(), getValue(property)));
    }
    protected static <T> T findNextInCollection(List<T> values, T current) {
        int index = values.indexOf(current);
        return values.get((index + 1) % values.size());
    }
    public String toString() {
        StringBuilder text = new StringBuilder(String.valueOf(owner));
        if (keys.length > 0) {
            text.append('[');
            for (int i = 0; i < keys.length; i++) {
                text.append(i == 0 ? "" : ",").append(keys[i].getName()).append('=').append(values[i]);
            }
            text.append(']');
        }
        return text.toString();
    }
    public boolean equals(Object obj) {
        return this == obj;
    }
    public int hashCode() {
        return System.identityHashCode(this);
    }
    public <T extends Comparable<T>> T getValueOrElse(Property<T> property, T defaultValue) {
        return hasProperty(property) ? getValue(property) : defaultValue;
    }
    void initializeNeighbors(S[][] neighbors) {
        this.neighbors = neighbors;
    }
    public boolean isSingletonState() {
        return getProperties().isEmpty();
    }
    protected static <O, S extends StateHolder<O, S>> Codec<S> codec(Codec<O> ownerCodec, Function<O, S> defaultState, Function<O, StateDefinition<O, S>> stateDefinition) {
        return ownerCodec.xmap(defaultState, holder -> holder.owner);
    }
}

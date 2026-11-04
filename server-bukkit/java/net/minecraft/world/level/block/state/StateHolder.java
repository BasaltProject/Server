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
    public static final String ID_TAG = "Name";
    public static final String PROPERTIES_TAG = "Properties";
    protected StateHolder(O owner, Property<?>[] propertyKeys, Comparable<?>[] propertyValues) {
        this.owner = owner;
    }
    public <T extends Comparable<T>> S cycle(Property<T> property) {
        return setValue(property, findNextInCollection(property.getPossibleValues(), getValue(property)));
    }
    protected static <T> T findNextInCollection(List<T> values, T current) {
        int index = values.indexOf(current);
        return values.get((index + 1) % values.size());
    }
    public String toString() {
        return super.toString();
    }
    public boolean equals(Object obj) {
        return super.equals(obj);
    }
    public int hashCode() {
        return super.hashCode();
    }
    public <T extends Comparable<T>> T getValueOrElse(Property<T> property, T defaultValue) {
        return hasProperty(property) ? getValue(property) : defaultValue;
    }
    void initializeNeighbors(S[][] neighbors) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateHolder#initializeNeighbors");
    }
    public boolean isSingletonState() {
        return getProperties().isEmpty();
    }
    protected static <O, S extends StateHolder<O, S>> Codec<S> codec(Codec<O> ownerCodec, Function<O, S> defaultState, Function<O, StateDefinition<O, S>> stateDefinition) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateHolder#codec");
    }
}

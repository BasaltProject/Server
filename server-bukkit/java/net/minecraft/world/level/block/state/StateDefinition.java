// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import net.minecraft.world.level.block.state.properties.Property;
import com.google.common.collect.ImmutableList;
import com.mojang.serialization.MapCodec;
import java.util.function.Function;

public class StateDefinition<O, S extends StateHolder<O, S>> {
    private final O owner;
    private final Map<String, Property<?>> propertiesByName = new TreeMap<>();
    private final List<S> states;

    public StateDefinition(O owner, List<Property<?>> properties, List<S> states) {
        this.owner = owner;
        for (Property<?> property : properties) {
            propertiesByName.put(property.getName(), property);
        }
        this.states = states;
    }

    public List<S> getPossibleStates() {
        return states;
    }

    public S any() {
        return states.get(0);
    }

    public O getOwner() {
        return owner;
    }

    public Collection<Property<?>> getProperties() {
        return propertiesByName.values();
    }

    public Property<?> getProperty(String name) {
        return propertiesByName.get(name);
    }
    protected StateDefinition(Function<O, S> defaultState, O owner, Factory<O, S> factory, Map<String, Property<?>> properties) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition#<init>");
    }
    public MapCodec<S> propertiesCodec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition#propertiesCodec");
    }
    public String toString() {
        return super.toString();
    }
    public boolean isSingletonState() {
        return false;
    }
    public static class Builder<O, S extends StateHolder<O, S>> {
        public Builder(O owner) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition.Builder#<init>");
        }
        public Builder<O, S> add(Property<?>... properties) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition.Builder#add");
        }
        public StateDefinition<O, S> create(Function<O, S> defaultState, Factory<O, S> factory) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition.Builder#create");
        }
    }
    static record StateCollection<S extends StateHolder<?, ?>>(Map<List<Comparable<?>>, S> statesByValues, Map<List<Comparable<?>>, S[]> statesByPivotCache) {
        public S[][] fillNeighborsForState(Property<?>[] propertyKeys, List<Comparable<?>> propertyValues) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.StateDefinition.StateCollection#fillNeighborsForState");
        }
        static enum Wildcard {
            INSTANCE;
        }
    }
    public interface Factory<O, S> {
        S create(O type, Property<?>[] propertyKeys, Comparable<?>[] propertyValues);
    }
}

package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import net.minecraft.world.level.block.state.properties.Property;

public class StateDefinition<O, S> {
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
}

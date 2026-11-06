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
    private List<S> states;

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
    @SuppressWarnings("unchecked")
    protected StateDefinition(Function<O, S> defaultState, O owner, Factory<O, S> factory, Map<String, Property<?>> properties) {
        this.owner = owner;
        this.propertiesByName.putAll(properties);
        List<Property<?>> keys = new java.util.ArrayList<>(properties.values());
        List<List<Comparable<?>>> combinations = new java.util.ArrayList<>();
        combinations.add(List.of());
        for (Property<?> property : keys) {
            List<List<Comparable<?>>> next = new java.util.ArrayList<>();
            for (List<Comparable<?>> prefix : combinations) {
                for (Comparable<?> value : property.getPossibleValues()) {
                    List<Comparable<?>> extended = new java.util.ArrayList<>(prefix);
                    extended.add(value);
                    next.add(extended);
                }
            }
            combinations = next;
        }
        Property<?>[] keyArray = keys.toArray(new Property<?>[0]);
        Map<List<Comparable<?>>, S> byValues = new java.util.LinkedHashMap<>();
        List<S> created = new java.util.ArrayList<>();
        for (List<Comparable<?>> values : combinations) {
            S state = factory.create(owner, keyArray, values.toArray(new Comparable<?>[0]));
            byValues.put(values, state);
            created.add(state);
        }
        for (S state : created) {
            Comparable<?>[] current = new Comparable<?>[keyArray.length];
            for (int i = 0; i < keyArray.length; i++) {
                current[i] = state.getValues().get(keyArray[i]);
            }
            S[][] table = (S[][]) new StateHolder[keyArray.length][];
            for (int i = 0; i < keyArray.length; i++) {
                List<? extends Comparable<?>> possible = keyArray[i].getPossibleValues();
                table[i] = (S[]) new StateHolder[possible.size()];
                for (int slot = 0; slot < possible.size(); slot++) {
                    List<Comparable<?>> key = new java.util.ArrayList<>(java.util.Arrays.asList(current));
                    key.set(i, possible.get(slot));
                    table[i][slot] = byValues.get(key);
                }
            }
            state.initializeNeighbors(table);
        }
        this.states = List.copyOf(created);
    }
    public MapCodec<S> propertiesCodec() {
        return com.mojang.serialization.MapCodec.of(
            com.mojang.serialization.Encoder.empty(),
            new com.mojang.serialization.MapDecoder.Implementation<S>() {
                public <T> com.mojang.serialization.DataResult<S> decode(com.mojang.serialization.DynamicOps<T> ops, com.mojang.serialization.MapLike<T> input) {
                    S state = any();
                    for (Property<?> property : getProperties()) {
                        T raw = input.get(property.getName());
                        if (raw != null) {
                            com.mojang.serialization.DataResult<S> next = property.parseValue(ops, state, raw);
                            if (next.result().isEmpty()) {
                                return next;
                            }
                            state = next.result().get();
                        }
                    }
                    return com.mojang.serialization.DataResult.success(state);
                }

                public <T> java.util.stream.Stream<T> keys(com.mojang.serialization.DynamicOps<T> ops) {
                    return getProperties().stream().map(property -> ops.createString(property.getName()));
                }
            });
    }
    public String toString() {
        return "StateDefinition{owner=" + owner + ", properties=" + propertiesByName.keySet() + "}";
    }
    public boolean isSingletonState() {
        return states.size() == 1;
    }
    public static class Builder<O, S extends StateHolder<O, S>> {
        private final O owner;
        private final Map<String, Property<?>> properties = new java.util.LinkedHashMap<>();
        public Builder(O owner) {
            this.owner = owner;
        }
        public Builder<O, S> add(Property<?>... properties) {
            for (Property<?> property : properties) {
                if (this.properties.put(property.getName(), property) != null) {
                    throw new IllegalArgumentException("Duplicate property " + property.getName() + " on " + owner);
                }
            }
            return this;
        }
        public StateDefinition<O, S> create(Function<O, S> defaultState, Factory<O, S> factory) {
            return new StateDefinition<>(defaultState, owner, factory, new TreeMap<>(properties));
        }
    }
    static record StateCollection<S extends StateHolder<?, ?>>(Map<List<Comparable<?>>, S> statesByValues, Map<List<Comparable<?>>, S[]> statesByPivotCache) {
        @SuppressWarnings("unchecked")
        public S[][] fillNeighborsForState(Property<?>[] propertyKeys, List<Comparable<?>> propertyValues) {
            S[][] table = (S[][]) new StateHolder[propertyKeys.length][];
            for (int i = 0; i < propertyKeys.length; i++) {
                List<? extends Comparable<?>> possible = propertyKeys[i].getPossibleValues();
                table[i] = (S[]) new StateHolder[possible.size()];
                for (int slot = 0; slot < possible.size(); slot++) {
                    List<Comparable<?>> key = new java.util.ArrayList<>(propertyValues);
                    key.set(i, possible.get(slot));
                    table[i][slot] = statesByValues.get(key);
                }
            }
            return table;
        }
        static enum Wildcard {
            INSTANCE;
        }
    }
    public interface Factory<O, S> {
        S create(O type, Property<?>[] propertyKeys, Comparable<?>[] propertyValues);
    }
}

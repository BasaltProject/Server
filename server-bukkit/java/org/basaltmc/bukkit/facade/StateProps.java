// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.facade;

import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;

/** Sets block state properties by name, for blocks whose property constants are not exposed by the facade. */
public final class StateProps {
    private StateProps() {}

    public static BlockState with(BlockState state, String name, Object value) {
        for (Property<?> property : state.getProperties()) {
            if (property.getName().equals(name)) {
                return set(state, property, String.valueOf(value));
            }
        }
        return state;
    }

    private static <T extends Comparable<T>> BlockState set(BlockState state, Property<T> property, String text) {
        return property.getValue(text).map(found -> state.setValue(property, found)).orElse(state);
    }
}
